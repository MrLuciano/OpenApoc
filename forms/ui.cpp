#include "forms/ui.h"
#include "forms/forms.h"
#include "framework/apocresources/apocfont.h"
#include "framework/framework.h"
#include "framework/trace.h"
#include "library/sp.h"
#include <tinyxml2.h>

namespace OpenApoc
{

up<UI> UI::instance = nullptr;

UI &UI::getInstance()
{
	if (!UI::instance)
	{
		UI::instance.reset(new UI);
		UI::instance->Load(fw().Settings->getString("GameRules"));
	}
	return *UI::instance;
}

UI &ui() { return UI::getInstance(); };

void UI::unload() { UI::instance.reset(nullptr); }

UI::UI() : fonts(), forms() {}

void UI::Load(UString CoreXMLFilename) { ParseXMLDoc(CoreXMLFilename); }

UI::~UI() {}

void UI::ParseXMLDoc(UString XMLFilename)
{
	TRACE_FN_ARGS1("XMLFilename", XMLFilename);
	tinyxml2::XMLDocument doc;
	tinyxml2::XMLElement *node;
	auto file = fw().data->fs.open(XMLFilename);
	if (!file)
	{
		LogError("Failed to open XML file \"%s\"", XMLFilename.c_str());
	}

	LogInfo("Loading XML file \"%s\" - found at \"%s\"", XMLFilename.c_str(),
	        file.systemPath().c_str());

	auto xmlText = file.readAll();
	if (!xmlText)
	{
		LogError("Failed to read in XML file \"%s\"", XMLFilename.c_str());
	}

	auto err = doc.Parse(xmlText.get(), file.size());

	if (err != tinyxml2::XML_SUCCESS)
	{
		LogError("Failed to parse XML file \"%s\" - \"%s\" \"%s\"", XMLFilename.c_str(),
		         doc.GetErrorStr1(), doc.GetErrorStr2());
		return;
	}

	node = doc.RootElement();

	if (!node)
	{
		LogError("Failed to parse XML file \"%s\" - no root element", XMLFilename.c_str());
		return;
	}

	UString nodename = node->Name();

	if (nodename == "openapoc")
	{
		for (node = node->FirstChildElement(); node != nullptr; node = node->NextSiblingElement())
		{
			ApplyAliases(node);
			nodename = node->Name();
			if (nodename == "game")
			{
				ParseGameXML(node);
			}
			else if (nodename == "form")
			{
				ParseFormXML(node);
			}
			else if (nodename == "apocfont")
			{
				UString fontName = node->Attribute("name");
				if (fontName == "")
				{
					LogError("apocfont element with no name");
					continue;
				}
				auto font = ApocalypseFont::loadFont(node);
				if (!font)
				{
					LogError("apocfont element \"%s\" failed to load", fontName.c_str());
					continue;
				}

				if (this->fonts.find(fontName) != this->fonts.end())
				{
					LogError("multiple fonts with name \"%s\"", fontName.c_str());
					continue;
				}
				this->fonts[fontName] = font;
			}
			else if (nodename == "alias")
			{
				aliases[UString(node->Attribute("id"))] = UString(node->GetText());
			}
			else
			{
				LogError("Unknown XML element \"%s\"", nodename.c_str());
			}
		}
	}
}

void UI::ParseGameXML(tinyxml2::XMLElement *Source)
{
	tinyxml2::XMLElement *node;
	UString nodename;

	for (node = Source->FirstChildElement(); node != nullptr; node = node->NextSiblingElement())
	{
		nodename = node->Name();
		if (nodename == "title")
		{
			fw().Display_SetTitle(node->GetText());
		}
		if (nodename == "include")
		{
			ParseXMLDoc(node->GetText());
		}
	}
}

void UI::ParseFormXML(tinyxml2::XMLElement *Source)
{
	auto form = mksp<Form>();
	form->ReadFormStyle(Source);
	forms[Source->Attribute("id")] = form;
}

sp<Form> UI::GetForm(UString ID)
{
	return std::dynamic_pointer_cast<Form>(forms[ID]->CopyTo(nullptr));
}

sp<BitmapFont> UI::GetFont(UString FontData)
{
	if (fonts.find(FontData) == fonts.end())
	{
		LogError("Missing font \"%s\"", FontData.c_str());
		return nullptr;
	}
	return fonts[FontData];
}

void UI::ApplyAliases(tinyxml2::XMLElement *Source)
{
	if (aliases.empty())
	{
		return;
	}

	const tinyxml2::XMLAttribute *attr = Source->FirstAttribute();

	while (attr != nullptr)
	{
		// Is the attribute value the same as an alias? If so, replace with alias' value
		if (aliases.find(UString(attr->Value())) != aliases.end())
		{
			LogInfo("%s attribute \"%s\" value \"%s\" matches alias \"%s\"", Source->Name(),
			        attr->Name(), attr->Value(), aliases[UString(attr->Value())].c_str());
			Source->SetAttribute(attr->Name(), aliases[UString(attr->Value())].c_str());
		}

		attr = attr->Next();
	}

	// Replace inner text
	if (Source->GetText() != nullptr && aliases.find(UString(Source->GetText())) != aliases.end())
	{
		LogInfo("%s  value \"%s\" matches alias \"%s\"", Source->Name(), Source->GetText(),
		        aliases[UString(Source->GetText())].c_str());
		Source->SetText(aliases[UString(Source->GetText())].c_str());
	}

	// Recurse down tree
	tinyxml2::XMLElement *child = Source->FirstChildElement();
	while (child != nullptr)
	{
		ApplyAliases(child);
		child = child->NextSiblingElement();
	}
}

std::vector<UString> UI::GetFormIDs()
{
	std::vector<UString> result;

	for (auto idx = forms.begin(); idx != forms.end(); idx++)
	{
		result.push_back(idx->first);
	}

	return result;
}

}; // namespace OpenApoc