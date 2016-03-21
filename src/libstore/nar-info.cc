#include "crypto.hh"
#include "globals.hh"
#include "nar-info.hh"

namespace nix {

NarInfo::NarInfo(const std::string & s, const std::string & whence)
{
    auto corrupt = [&]() {
        throw Error("NAR info file ‘%1%’ is corrupt");
    };

    auto parseHashField = [&](const string & s) {
        string::size_type colon = s.find(':');
        if (colon == string::npos) corrupt();
        HashType ht = parseHashType(string(s, 0, colon));
        if (ht == htUnknown) corrupt();
        return parseHash16or32(ht, string(s, colon + 1));
    };

    size_t pos = 0;
    while (pos < s.size()) {

        size_t colon = s.find(':', pos);
        if (colon == std::string::npos) corrupt();

        std::string name(s, pos, colon - pos);

        size_t eol = s.find('\n', colon + 2);
        if (eol == std::string::npos) corrupt();

        std::string value(s, colon + 2, eol - colon - 2);

        if (name == "StorePath") {
            if (!isStorePath(value)) corrupt();
            path = value;
        }
        else if (name == "URL")
            url = value;
        else if (name == "Compression")
            compression = value;
        else if (name == "FileHash")
            fileHash = parseHashField(value);
        else if (name == "FileSize") {
            if (!string2Int(value, fileSize)) corrupt();
        }
        else if (name == "NarHash")
            narHash = parseHashField(value);
        else if (name == "NarSize") {
            if (!string2Int(value, narSize)) corrupt();
        }
        else if (name == "References") {
            auto refs = tokenizeString<Strings>(value, " ");
            if (!references.empty()) corrupt();
            for (auto & r : refs) {
                auto r2 = settings.nixStore + "/" + r;
                if (!isStorePath(r2)) corrupt();
                references.insert(r2);
            }
        }
        else if (name == "Deriver") {
            auto p = settings.nixStore + "/" + value;
            if (!isStorePath(p)) corrupt();
            deriver = p;
        }
        else if (name == "System")
            system = value;
        else if (name == "Sig")
            sigs.insert(value);

        pos = eol + 1;
    }

    if (compression == "") compression = "bzip2";

    if (path.empty() || url.empty()) corrupt();
}

std::string NarInfo::to_string() const
{
    std::string res;
    res += "StorePath: " + path + "\n";
    res += "URL: " + url + "\n";
    assert(compression != "");
    res += "Compression: " + compression + "\n";
    assert(fileHash.type == htSHA256);
    res += "FileHash: sha256:" + printHash32(fileHash) + "\n";
    res += "FileSize: " + std::to_string(fileSize) + "\n";
    assert(narHash.type == htSHA256);
    res += "NarHash: sha256:" + printHash32(narHash) + "\n";
    res += "NarSize: " + std::to_string(narSize) + "\n";

    res += "References: " + concatStringsSep(" ", shortRefs()) + "\n";

    if (!deriver.empty())
        res += "Deriver: " + baseNameOf(deriver) + "\n";

    if (!system.empty())
        res += "System: " + system + "\n";

    for (auto sig : sigs)
        res += "Sig: " + sig + "\n";

    return res;
}

std::string NarInfo::fingerprint() const
{
    return
        "1;" + path + ";"
        + printHashType(narHash.type) + ":" + printHash32(narHash) + ";"
        + std::to_string(narSize) + ";"
        + concatStringsSep(",", references);
}

Strings NarInfo::shortRefs() const
{
    Strings refs;
    for (auto & r : references)
        refs.push_back(baseNameOf(r));
    return refs;
}

void NarInfo::sign(const SecretKey & secretKey)
{
    sigs.insert(secretKey.signDetached(fingerprint()));
}

unsigned int NarInfo::checkSignatures(const PublicKeys & publicKeys) const
{
    unsigned int good = 0;
    for (auto & sig : sigs)
        if (verifyDetached(fingerprint(), sig, publicKeys))
            good++;
    return good;
}

}
