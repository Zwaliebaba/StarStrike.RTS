#pragma once

class Strings
{
	public:
		static void Startup();
		static void Shutdown();
		static void SetLanguage(const hstring& _language);

		static std::wstring Get(const hstring& _stringId, const hstring& _class = L"Strings");
		static std::string Get(const std::string& _stringId, const std::string& _class = "Strings");
};

