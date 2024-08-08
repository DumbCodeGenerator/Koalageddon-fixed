#include "pch.h"
#include "UpdateChecker.h"
#include "Logger.h"
#include "constants.h"

void UpdateChecker::checkForUpdates()
{
    std::thread([]() {
        // Fetch offsets
        auto r = fetch(latest_release_url_api);

        if (r.status_code != 200)
        {
            // Manually convert error code to string
            logger->error(
                "Failed to check for updates. StatusCode: {}. Message: {}",
                r.status_code, r.error.message
            );
            return;
        }

        auto release = json::parse(r.text, nullptr, true, true);
        std::string tagName = release["tag_name"];

        auto currentVersionTag = fmt::format("v{}", VERSION);

        if (tagName != currentVersionTag)
        {
            // Convert wide string to narrow string
            logger->warn("A new Koalageddon version {} has been released. Get it from: {}", tagName, std::string(latest_release_url, latest_release_url + wcslen(latest_release_url)));
        }
        }).detach();
}

