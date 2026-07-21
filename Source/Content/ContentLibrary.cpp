#include "ContentLibrary.h"

namespace
{
juce::String slugify(const juce::String& text)
{
    auto slug = text.trim().toLowerCase();
    slug = slug.retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789-_ ");
    slug = slug.replace(" ", "-");
    while (slug.contains("--"))
        slug = slug.replace("--", "-");
    slug = slug.trimCharactersAtStart("-");
    slug = slug.trimCharactersAtEnd("-");
    return slug.isNotEmpty() ? slug : "item";
}
}

juce::String ContentLibrary::originName(Origin origin)
{
    switch (origin)
    {
        case Origin::builtIn: return "Built-In";
        case Origin::downloaded: return "LagDaemon";
        case Origin::user: return "My Library";
        case Origin::remote: return "Remote";
    }

    return "Unknown";
}

juce::String ContentLibrary::accessName(AccessState accessState)
{
    switch (accessState)
    {
        case AccessState::installed: return "Installed";
        case AccessState::available: return "Available";
        case AccessState::locked: return "Locked";
    }

    return "Unknown";
}

juce::String ContentLibrary::makeItemId(const juce::File& file, Origin origin)
{
    return originName(origin).toLowerCase().retainCharacters("abcdefghijklmnopqrstuvwxyz") + "-" + slugify(file.getFileNameWithoutExtension());
}

juce::String ContentLibrary::inferTypeFromFile(const juce::File& file)
{
    auto extension = file.getFileExtension().toLowerCase();
    if (extension == ".cspatch")
        return "patch";
    if (extension == ".cspack")
        return "pack";
    if (extension == ".wav" || extension == ".aif" || extension == ".aiff" || extension == ".flac" || extension == ".mp3" || extension == ".ogg")
        return "audio";

    return "file";
}

juce::String ContentLibrary::inferCategoryFromFile(const juce::File& file)
{
    auto type = inferTypeFromFile(file);
    if (type == "patch")
        return "Patches";
    if (type == "pack")
        return "Content Packs";
    if (type == "audio")
        return "Audio Assets";

    return "Other";
}

void ContentLibrary::appendFilesFromDirectory(juce::Array<Item>& destination, const juce::File& directory, Origin origin)
{
    if (! directory.isDirectory())
        return;

    juce::Array<juce::File> files;
    directory.findChildFiles(files, juce::File::findFiles, true, "*");

    for (const auto& file : files)
    {
        auto displayName = file.getFileNameWithoutExtension();
        auto itemId = makeItemId(file, origin);

        if (origin == Origin::downloaded)
        {
            auto marker = displayName.indexOf("__");
            if (marker > 0)
            {
                itemId = displayName.substring(0, marker);
                displayName = displayName.substring(marker + 2);
            }
        }

        Item item;
        item.id = itemId;
        item.name = displayName;
        item.type = inferTypeFromFile(file);
        item.category = inferCategoryFromFile(file);
        item.description = "Local " + item.type + " from " + originName(origin) + ".";
        item.requiredTier = origin == Origin::downloaded ? "free or better" : "none";
        item.origin = origin;
        item.accessState = AccessState::installed;
        item.file = file;
        item.fileSizeBytes = file.getSize();
        destination.add(item);
    }
}

bool ContentLibrary::loadFromStorage(const juce::File& builtInDirectory,
                                     const juce::File& downloadedDirectory,
                                     const juce::File& userDirectory,
                                     const juce::File& manifestFile,
                                     juce::String& errorMessage)
{
    struct ItemSorter
    {
        static int compareElements(const Item& a, const Item& b)
        {
            if (a.origin != b.origin)
                return (int) a.origin < (int) b.origin ? -1 : 1;

            return a.name.compareNatural(b.name);
        }
    };

    items.clearQuick();
    appendFilesFromDirectory(items, builtInDirectory, Origin::builtIn);
    appendFilesFromDirectory(items, downloadedDirectory, Origin::downloaded);
    appendFilesFromDirectory(items, userDirectory, Origin::user);

    ItemSorter sorter;
    items.sort(sorter);

    return writeManifestSnapshot(manifestFile, errorMessage);
}

juce::String ContentLibrary::createSummaryText() const
{
    int builtInCount = 0;
    int downloadedCount = 0;
    int userCount = 0;

    for (const auto& item : items)
    {
        if (item.origin == Origin::builtIn)
            ++builtInCount;
        else if (item.origin == Origin::downloaded)
            ++downloadedCount;
        else if (item.origin == Origin::user)
            ++userCount;
    }

    return "Library ready: "
         + juce::String(items.size()) + " items"
         + "  |  Built-In " + juce::String(builtInCount)
         + "  |  LagDaemon " + juce::String(downloadedCount)
         + "  |  Mine " + juce::String(userCount);
}

bool ContentLibrary::writeManifestSnapshot(const juce::File& manifestFile, juce::String& errorMessage) const
{
    auto* root = new juce::DynamicObject();
    juce::Array<juce::var> manifestItems;

    for (const auto& item : items)
    {
        auto* object = new juce::DynamicObject();
        object->setProperty("id", item.id);
        object->setProperty("name", item.name);
        object->setProperty("type", item.type);
        object->setProperty("category", item.category);
        object->setProperty("description", item.description);
        object->setProperty("requiredTier", item.requiredTier);
        object->setProperty("version", item.version);
        object->setProperty("origin", originName(item.origin));
        object->setProperty("accessState", accessName(item.accessState));
        object->setProperty("path", item.file.getFullPathName());
        object->setProperty("size", (double) item.fileSizeBytes);
        manifestItems.add(juce::var(object));
    }

    root->setProperty("generatedAt", juce::Time::getCurrentTime().toISO8601(true));
    root->setProperty("items", juce::var(manifestItems));

    manifestFile.getParentDirectory().createDirectory();
    if (! manifestFile.replaceWithText(juce::JSON::toString(juce::var(root), true)))
    {
        errorMessage = "Could not write the local content manifest.";
        return false;
    }

    return true;
}
