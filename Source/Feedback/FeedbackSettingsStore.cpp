#include "FeedbackSettingsStore.h"
#include <creation/services/SuiteVfsJsonStore.h>

namespace creation_station
{
namespace
{
constexpr const char* kSettingsPath = "feedback-settings.json";
}

FeedbackSettingsStore::Settings FeedbackSettingsStore::load()
{
    Settings settings;
    juce::String loadError;
    auto json = creation::services::SuiteVfsJsonStore::loadJson(kSettingsPath, loadError);
    if (auto* object = json.getDynamicObject())
    {
        settings.installId = object->getProperty("installId").toString();
        settings.feedbackOptIn = (bool) object->getProperty("feedbackOptIn");
        settings.metricsOptIn = (bool) object->getProperty("metricsOptIn");
    }

    if (settings.installId.trim().isEmpty())
    {
        settings.installId = juce::Uuid().toString();
        juce::String saveError;
        save(settings, saveError); // best-effort -- if this fails, a fresh id is generated next launch
    }

    return settings;
}

bool FeedbackSettingsStore::save(const Settings& settings, juce::String& errorMessage)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("installId", settings.installId);
    object->setProperty("feedbackOptIn", settings.feedbackOptIn);
    object->setProperty("metricsOptIn", settings.metricsOptIn);
    return creation::services::SuiteVfsJsonStore::saveJson(kSettingsPath, juce::var(object), errorMessage);
}
}
