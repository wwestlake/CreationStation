#include "Branding.h"

namespace branding
{
namespace
{
struct BadgeAsset
{
    const char* tierId;
    const char* displayName;
    const char* svg;
};

constexpr BadgeAsset badgeAssets[] =
{
    {
        "curious-mind",
        "Curious Mind",
        R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100" width="100" height="100">
  <circle cx="50" cy="50" r="45" fill="#2c3e50" />
  <circle cx="50" cy="50" r="35" fill="none" stroke="#7f8c8d" stroke-width="4" stroke-dasharray="8 4" />
  <circle cx="50" cy="50" r="15" fill="#3498db" />
  <path d="M 50 15 L 50 35 M 50 65 L 50 85 M 15 50 L 35 50 M 65 50 L 85 50" stroke="#7f8c8d" stroke-width="2" />
</svg>)svg"
    },
    {
        "lab-assistant",
        "Lab Assistant",
        R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100" width="100" height="100">
  <rect x="5" y="5" width="90" height="90" rx="10" fill="#2c3e50" />
  <path d="M 20 30 L 40 50 L 20 70" fill="none" stroke="#2ecc71" stroke-width="6" stroke-linecap="round" stroke-linejoin="round" />
  <line x1="50" y1="70" x2="80" y2="70" stroke="#2ecc71" stroke-width="6" stroke-linecap="round" />
  <line x1="10" y1="10" x2="90" y2="90" stroke="#7f8c8d" stroke-width="2" opacity="0.5" />
</svg>)svg"
    },
    {
        "research-fellow",
        "Research Fellow",
        R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100" width="100" height="100">
  <circle cx="50" cy="50" r="45" fill="#2c3e50" />
  <path d="M 20 80 L 80 80 L 80 20" fill="none" stroke="#d35400" stroke-width="4" stroke-linecap="round" stroke-linejoin="round" />
  <path d="M 20 80 L 70 30" fill="none" stroke="#e67e22" stroke-width="4" stroke-linecap="round" />
  <circle cx="70" cy="30" r="4" fill="#f39c12" />
  <path d="M 20 80 L 40 20" fill="none" stroke="#e67e22" stroke-width="4" stroke-linecap="round" />
  <circle cx="40" cy="20" r="4" fill="#f39c12" />
</svg>)svg"
    },
    {
        "professor",
        "Professor",
        R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100" width="100" height="100">
  <circle cx="50" cy="50" r="45" fill="#2c3e50" />
  <polygon points="50,15 80,35 80,75 50,95 20,75 20,35" fill="none" stroke="#bdc3c7" stroke-width="3" stroke-linejoin="round" />
  <polygon points="50,35 65,45 65,65 50,75 35,65 35,45" fill="none" stroke="#ecf0f1" stroke-width="2" stroke-linejoin="round" />
  <line x1="50" y1="15" x2="50" y2="35" stroke="#bdc3c7" stroke-width="2" />
  <line x1="80" y1="35" x2="65" y2="45" stroke="#bdc3c7" stroke-width="2" />
  <line x1="80" y1="75" x2="65" y2="65" stroke="#bdc3c7" stroke-width="2" />
  <line x1="50" y1="95" x2="50" y2="75" stroke="#bdc3c7" stroke-width="2" />
  <line x1="20" y1="75" x2="35" y2="65" stroke="#bdc3c7" stroke-width="2" />
  <line x1="20" y1="35" x2="35" y2="45" stroke="#bdc3c7" stroke-width="2" />
  <circle cx="50" cy="50" r="6" fill="#ecf0f1" />
</svg>)svg"
    },
    {
        "dean",
        "Dean",
        R"svg(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100" width="100" height="100">
  <circle cx="50" cy="50" r="45" fill="#2c3e50" />
  <path d="M 50 10 L 85 50 L 50 90 L 15 50 Z" fill="none" stroke="#f1c40f" stroke-width="3" stroke-linejoin="round" />
  <path d="M 50 10 L 70 50 L 50 90 L 30 50 Z" fill="none" stroke="#f39c12" stroke-width="2" stroke-linejoin="round" />
  <line x1="15" y1="50" x2="85" y2="50" stroke="#f1c40f" stroke-width="3" />
  <line x1="50" y1="10" x2="50" y2="90" stroke="#f1c40f" stroke-width="3" />
  <circle cx="50" cy="50" r="8" fill="#f1c40f" />
  <circle cx="85" cy="50" r="4" fill="#f39c12" />
  <circle cx="15" cy="50" r="4" fill="#f39c12" />
  <circle cx="50" cy="10" r="4" fill="#f39c12" />
  <circle cx="50" cy="90" r="4" fill="#f39c12" />
</svg>)svg"
    }
};

const BadgeAsset* findBadgeAsset(const juce::String& tierId)
{
    for (const auto& asset : badgeAssets)
        if (tierId == asset.tierId)
            return &asset;

    return nullptr;
}
}

juce::String getPatreonTierDisplayName(const juce::String& tierId)
{
    if (const auto* asset = findBadgeAsset(tierId))
        return asset->displayName;

    return {};
}

juce::String getBestPatreonTierId(const juce::StringArray& entitlements)
{
    constexpr auto badgeCount = static_cast<int>(sizeof(badgeAssets) / sizeof(badgeAssets[0]));
    for (int index = badgeCount - 1; index >= 0; --index)
        if (entitlements.contains(badgeAssets[static_cast<size_t>(index)].tierId))
            return badgeAssets[static_cast<size_t>(index)].tierId;

    return {};
}

juce::Image createPatreonBadgeImage(const juce::String& tierId, int size)
{
    const auto* asset = findBadgeAsset(tierId);
    if (asset == nullptr)
        return {};

    auto xml = juce::parseXML(juce::String::fromUTF8(asset->svg));
    if (xml == nullptr)
        return {};

    auto drawable = juce::Drawable::createFromSVG(*xml);
    if (drawable == nullptr)
        return {};

    juce::Image image(juce::Image::ARGB, size, size, true);
    juce::Graphics g(image);
    drawable->drawWithin(g, image.getBounds().toFloat(), juce::RectanglePlacement::centred, 1.0f);
    return image;
}

juce::Image createCreationStationLogoImage(int size)
{
    const juce::String svg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" width="256" height="256" viewBox="0 0 256 256">
  <defs>
    <linearGradient id="bg" x1="24" y1="20" x2="232" y2="236" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#111a2f"/>
      <stop offset="100%" stop-color="#06070c"/>
    </linearGradient>
    <linearGradient id="accent" x1="36" y1="36" x2="220" y2="220" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#56f4ff"/>
      <stop offset="45%" stop-color="#7f7dff"/>
      <stop offset="100%" stop-color="#ff5fc8"/>
    </linearGradient>
  </defs>
  <rect x="12" y="12" width="232" height="232" rx="52" fill="url(#bg)"/>
  <rect x="28" y="28" width="200" height="200" rx="40" fill="none" stroke="url(#accent)" stroke-width="4" opacity="0.55"/>
  <path d="M66 154c14-1 25-12 35-24 11-13 20-27 33-39 12-10 27-16 44-16 19 0 34 6 52 19"
        fill="none" stroke="url(#accent)" stroke-width="13" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M66 176c15 0 27-7 38-18 12-12 21-26 33-38 14-14 31-21 53-21"
        fill="none" stroke="#d8e2ff" stroke-width="8" stroke-linecap="round" stroke-linejoin="round" opacity="0.9"/>
  <circle cx="72" cy="154" r="8" fill="#56f4ff"/>
  <circle cx="126" cy="114" r="8" fill="#7f7dff"/>
  <circle cx="182" cy="90" r="8" fill="#ff5fc8"/>
  <path d="M86 76h22l20 32 20-44 18 64 18-28h20"
        fill="none" stroke="#9fb3ff" stroke-width="7" stroke-linecap="round" stroke-linejoin="round" opacity="0.85"/>
  <path d="M78 188h100" stroke="#2f3952" stroke-width="12" stroke-linecap="round" opacity="0.9"/>
  <path d="M104 188h28m14 0h14m10 0h18" stroke="#56f4ff" stroke-width="8" stroke-linecap="round"/>
  <path d="M186 178l22 10-22 10z" fill="#ff5fc8" opacity="0.95"/>
</svg>
)svg";

    auto xml = juce::parseXML(svg);
    if (xml == nullptr)
        return {};

    auto drawable = juce::Drawable::createFromSVG(*xml);
    if (drawable == nullptr)
        return {};

    juce::Image image(juce::Image::ARGB, size, size, true);
    juce::Graphics g(image);
    drawable->drawWithin(g, image.getBounds().toFloat(), juce::RectanglePlacement::centred, 1.0f);
    return image;
}

juce::Image createDjehutiRouterLogoImage(int size)
{
    const juce::String svg = R"svg(
<svg xmlns="http://www.w3.org/2000/svg" width="256" height="256" viewBox="0 0 256 256">
  <defs>
    <linearGradient id="bg" x1="28" y1="20" x2="228" y2="234" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#101724"/>
      <stop offset="100%" stop-color="#06070c"/>
    </linearGradient>
    <linearGradient id="accent" x1="28" y1="28" x2="228" y2="228" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#56f4ff"/>
      <stop offset="50%" stop-color="#7f7dff"/>
      <stop offset="100%" stop-color="#ff5fc8"/>
    </linearGradient>
  </defs>
  <rect x="12" y="12" width="232" height="232" rx="52" fill="url(#bg)"/>
  <rect x="28" y="28" width="200" height="200" rx="40" fill="none" stroke="url(#accent)" stroke-width="4" opacity="0.50"/>
  <path d="M66 98h60c18 0 30 9 30 25 0 14-10 24-26 26l28 36"
        fill="none" stroke="url(#accent)" stroke-width="13" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M66 158h52c10 0 19-3 27-9l24-18"
        fill="none" stroke="#d8e2ff" stroke-width="8" stroke-linecap="round" stroke-linejoin="round" opacity="0.92"/>
  <circle cx="70" cy="98" r="8" fill="#56f4ff"/>
  <circle cx="152" cy="123" r="8" fill="#7f7dff"/>
  <circle cx="182" cy="186" r="8" fill="#ff5fc8"/>
  <path d="M104 70h18l14 34 17-52 16 76 20-30h18"
        fill="none" stroke="#9fb3ff" stroke-width="7" stroke-linecap="round" stroke-linejoin="round" opacity="0.84"/>
  <path d="M88 186h78" stroke="#2f3952" stroke-width="12" stroke-linecap="round" opacity="0.9"/>
  <path d="M112 186h22m10 0h16" stroke="#56f4ff" stroke-width="8" stroke-linecap="round"/>
  <path d="M198 176l20 10-20 10z" fill="#ff5fc8" opacity="0.95"/>
  <path d="M176 72c14 8 24 20 30 36" fill="none" stroke="#56f4ff" stroke-width="5" stroke-linecap="round" opacity="0.45"/>
</svg>
)svg";

    auto xml = juce::parseXML(svg);
    if (xml == nullptr)
        return {};

    auto drawable = juce::Drawable::createFromSVG(*xml);
    if (drawable == nullptr)
        return {};

    juce::Image image(juce::Image::ARGB, size, size, true);
    juce::Graphics g(image);
    drawable->drawWithin(g, image.getBounds().toFloat(), juce::RectanglePlacement::centred, 1.0f);
    return image;
}

juce::Image createDjehutiRouterSplashImage()
{
    constexpr int width = 720;
    constexpr int height = 420;

    juce::Image image(juce::Image::ARGB, width, height, true);
    juce::Graphics g(image);

    g.fillAll(juce::Colour(0xff090b10));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff122033), 0.0f, 0.0f,
                                            juce::Colour(0xff090b10), 0.0f, (float) height, false));
    g.fillRoundedRectangle(image.getBounds().toFloat().reduced(14.0f), 30.0f);

    g.setColour(juce::Colour(0xff273451));
    g.drawRoundedRectangle(image.getBounds().toFloat().reduced(14.0f), 30.0f, 1.0f);

    auto logo = createDjehutiRouterLogoImage(180);
    g.drawImageWithin(logo, 58, 112, 180, 180, juce::RectanglePlacement::centred, false);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(34.0f).boldened());
    g.drawText("Djehuti Router", 274, 92, 380, 40, juce::Justification::left, false);

    g.setColour(juce::Colour(0xff9fb0c8));
    g.setFont(juce::Font(18.0f));
    g.drawText("Routing monitor, capture, and device paths",
               274, 136, 420, 28, juce::Justification::left, false);

    g.setColour(juce::Colour(0xff7fcfff));
    g.setFont(juce::Font(17.0f).boldened());
    g.drawText("DjeRoute keeps your signal flow visible.", 274, 176, 320, 26, juce::Justification::left, false);

    g.setColour(juce::Colour(0xff56f4ff));
    g.fillRoundedRectangle(274.0f, 218.0f, 340.0f, 8.0f, 4.0f);
    g.setColour(juce::Colour(0x403c4a66));
    g.fillRoundedRectangle(274.0f, 218.0f, 376.0f, 8.0f, 4.0f);

    g.setColour(juce::Colour(0xffd8e2ff));
    g.setFont(juce::Font(15.0f));
    g.drawText("Source • sink • preset • future driver layer",
               274, 258, 410, 24, juce::Justification::left, false);

    return image;
}

juce::Image createCreationStationSplashImage()
{
    constexpr int width = 720;
    constexpr int height = 420;

    juce::Image image(juce::Image::ARGB, width, height, true);
    juce::Graphics g(image);

    g.fillAll(juce::Colour(0xff090b10));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff15253b), 0.0f, 0.0f,
                                            juce::Colour(0xff090b10), 0.0f, (float) height, false));
    g.fillRoundedRectangle(image.getBounds().toFloat().reduced(14.0f), 30.0f);

    g.setColour(juce::Colour(0xff273451));
    g.drawRoundedRectangle(image.getBounds().toFloat().reduced(14.0f), 30.0f, 1.0f);

    g.setColour(juce::Colour(0x22394a6a));
    for (int i = 0; i < 7; ++i)
    {
        auto y = 74.0f + (float) (i * 40);
        g.drawLine(320.0f, y, 676.0f, y, 1.0f);
    }

    auto logo = createCreationStationLogoImage(180);
    g.drawImageWithin(logo, 58, 112, 180, 180, juce::RectanglePlacement::centred, false);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(34.0f).boldened());
    g.drawText("Creation Station", 274, 92, 380, 40, juce::Justification::left, false);

    g.setColour(juce::Colour(0xff9fb0c8));
    g.setFont(juce::Font(18.0f));
    g.drawText("Audio workstation Â· mixer Â· node graph Â· AI assist",
               274, 136, 380, 28, juce::Justification::left, false);

    g.setColour(juce::Colour(0xff7fcfff));
    g.setFont(juce::Font(17.0f).boldened());
    g.drawText("Loading your creative deck...", 274, 176, 280, 26, juce::Justification::left, false);

    g.setColour(juce::Colour(0xff56f4ff));
    g.fillRoundedRectangle(274.0f, 218.0f, 340.0f, 8.0f, 4.0f);
    g.setColour(juce::Colour(0x403c4a66));
    g.fillRoundedRectangle(274.0f, 218.0f, 376.0f, 8.0f, 4.0f);

    g.setColour(juce::Colour(0xffd8e2ff));
    g.setFont(juce::Font(15.0f));
    g.drawText("Banked mixer â€¢ xTouch control â€¢ VST hosting â€¢ DSP graph",
               274, 258, 410, 24, juce::Justification::left, false);

    return image;
}
}
