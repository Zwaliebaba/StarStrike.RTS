#include "pch.h"
#include <winrt/Microsoft.Windows.ApplicationModel.Resources.h>

using namespace Microsoft::Windows::ApplicationModel::Resources;

namespace
{
  ResourceManager g_resourceManager;
  ResourceContext g_context = {nullptr};
}

void Strings::Startup()
{
  const hstring regionCode = Windows::Globalization::Language::CurrentInputMethodLanguageTag();

  g_context = g_resourceManager.CreateResourceContext();
  SetLanguage(regionCode);
}

void Strings::Shutdown() {}

void Strings::SetLanguage(const hstring& _language) { std::ignore = g_context.QualifierValues().Insert(L"Language", _language); }

std::wstring Strings::Get(const hstring& _stringId, const hstring& _class)
{
  const hstring stringKey = _class + L"/" + _stringId;

  const auto resourceMap = g_resourceManager.MainResourceMap();

  if (const auto candidate = resourceMap.TryGetValue(stringKey, g_context); candidate != nullptr)
    return std::wstring(candidate.ValueAsString());
  return std::wstring(stringKey);
}

std::string Strings::Get(const std::string& _stringId, const std::string& _class)
{
  return to_string(Get(to_hstring(_stringId), to_hstring(_class)));
}
