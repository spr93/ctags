/*
 * Generated by ./misc/optlib2c from optlib/man.ctags, Don't edit this manually.
 */
#include "general.h"
#include "parse.h"
#include "routines.h"


static void initializeManParser (const langType language __unused__)
{
}

extern parserDefinition* ManParser (void)
{
	static const char *const extensions [] = {
		"1",
		"2",
		"3",
		"4",
		"5",
		"6",
		"7",
		"8",
		"9",
		NULL
	};

	static const char *const aliases [] = {
		NULL
	};

	static const char *const patterns [] = {
		NULL
	};

	static const tagRegexTable const ManTagRegexTable [] = {
		{"^\\.TH[[:space:]]{1,}\"([^\"]{1,})\".*", "\\1",
		"t,title,titles", "{exclusive}{icase}{scope=push}"},
		{"^\\.TH[[:space:]]{1,}([^[:space:]]{1,}).*", "\\1",
		"t,title,titles", "{exclusive}{icase}{scope=push}"},
		{"^\\.SH[[:space:]]{1,}\"([^\"]{1,})\".*", "\\1",
		"s,section,sections", "{exclusive}{icase}{scope=ref}"},
		{"^\\.SH[[:space:]]{1,}([^[:space:]]{1,}).*", "\\1",
		"s,section,sections", "{exclusive}{icase}{scope=ref}"},
	};


	parserDefinition* const def = parserNew ("man");

	def->enabled       = FALSE;
	def->extensions    = extensions;
	def->patterns      = patterns;
	def->aliases       = aliases;
	def->method        = METHOD_NOT_CRAFTED|METHOD_REGEX;
	def->tagRegexTable = ManTagRegexTable;
	def->tagRegexCount = ARRAY_SIZE(ManTagRegexTable);
	def->initialize    = initializeManParser;

	return def;
}

/*
 * Editor modelines  -  https://www.wireshark.org/tools/modelines.html
 *
 * Local variables:
 * c-basic-offset: 4
 * tab-width: 4
 * End:
 *
 * vi: set shiftwidth=4 tabstop=4:
 * :indentSize=4:tabSize=4:
 */
