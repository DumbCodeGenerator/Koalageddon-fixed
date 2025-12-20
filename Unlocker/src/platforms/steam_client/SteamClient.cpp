#include "pch.h"
#include "SteamClient.h"
#include "steam_client_hooks.h"
#include "constants.h"
#include "PatternMatcher.h"


bool SteamClient::fetchAndCachePatterns() const {
	logger->debug("Fetching SteamClient patterns");

	// Fetch offsets
	const auto res = fetch(steamclient_patterns_url);

	// Ensure that error code and message formatting is handled correctly
	if (res.status_code != 200) {
		logger->error(
			"Failed to fetch SteamClient patterns. ErrorCode: {}. StatusCode: {}. Message: {}",
			static_cast<int>(res.error.code),  // Convert to int for logging
			res.status_code,                   // Directly formattable
			res.error.message                  // Directly formattable
		);
		return false;
	}

	// Cache offsets
	if (!writeFileContents(PATTERNS_FILE_PATH, res.text)) {
		logger->error("Failed to cache SteamClient patterns");
		return false;
	}

	logger->info("SteamClient patterns were successfully fetched and cached");
	return true;
}


void SteamClient::readCachedPatterns() {
	logger->debug("Reading SteamClient patterns from cache");

	patterns.clear();

	auto text = readFileContents(PATTERNS_FILE_PATH);

	if (text.empty()) {
		logger->error("No cached SteamClient patterns were found");
		return;
	}

	try {
		// Parse json into our vector
		json::parse(text, nullptr, true, true).get_to(patterns);
	}
	catch (json::exception& ex) {
		logger->error(L"Error parsing {}: {}", PATTERNS_FILE_PATH.wstring(), stow(ex.what()));
		return;
	}

	logger->info("SteamClient patterns were successfully read from file");
}

void SteamClient::installHook(void* hookedFunc, const string funcName) {
	static auto moduleInfo = getModuleInfo(handle);
	auto& [lpBaseOfDll, SizeOfImage, EntryPoint] = moduleInfo;

	const auto& pattern = patterns[funcName];

	logger->debug("'{}' search pattern: '{}'", funcName, pattern);


	const auto t1 = std::chrono::high_resolution_clock::now();
	const auto origFuncAddress = PatternMatcher::scanInternal(static_cast<PCSTR>(lpBaseOfDll), SizeOfImage, pattern);
	const auto t2 = std::chrono::high_resolution_clock::now();

	const double elapsedTime = std::chrono::duration<double, std::milli>(t2 - t1).count();
	logger->debug("'{}' address: {}. Search time: {:.2f} ms", funcName, origFuncAddress, elapsedTime);

	if (origFuncAddress != nullptr)
		installDetourHook(hookedFunc, funcName.c_str(), origFuncAddress);
	else
		logger->error(
			"Failed to find the address of function: {}. "
			"You can report this error to the official forum topic.",
			funcName
		);
}

void SteamClient::installHooks() {
	logger->info("steamclient64.dll version: {}", getModuleVersion("steamclient64.dll"));

#define HOOK(FUNC) installHook(FUNC, #FUNC)  // NOLINT(cppcoreguidelines-macro-usage)

	// We first try to hook Family Sharing functions,
	// since it is critical to hook them before they are called
	if (config->platformRefs.Steam.unlock_shared_library) {
		HOOK(SharedLibraryStopPlaying);
		HOOK(FamilyGroupRunningApp);
	}
	if (config->platformRefs.Steam.unlock_dlc && !config->platformRefs.Steam.replicate) {
		HOOK(IsAppDLCEnabled);
		HOOK(IsSubscribedApp);
		HOOK(GetDLCDataByIndex);
	}
}

void SteamClient::platformInit() {
	logger->debug("Current process: {}, Steam process: {}", getCurrentProcessName(), config->platformRefs.Steam.process);
	if (!stringsAreEqual(getCurrentProcessName(), config->platformRefs.Steam.process, true)) {
		logger->debug("Ignoring hooks since this is not a Steam process");
		return;
	}

	fetchAndCachePatterns();
	readCachedPatterns();

	if (!patterns.empty()) {
		installHooks();
	}
	else {
		showFatalError(
			"Failed to initialize Steam platform since steamclient-patterns.json "
			"was not found in cache directory and could not be fetched from online source.",
			false,
			false
		);
	}
}

string SteamClient::getPlatformName() {
	return STEAM_CLIENT_NAME;
}

LPCWSTR SteamClient::getModuleName() {
	return STEAM_CLIENT;
}

Hooks& SteamClient::getPlatformHooks() {
	return hooks;
}
