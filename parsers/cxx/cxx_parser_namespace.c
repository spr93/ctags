/*
*   Copyright (c) 2016, Szymon Tomasz Stefanek
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License version 2 or (at your option) any later version.
*
*   This module contains functions for parsing and scanning C++ source files
*/
#include "cxx_parser.h"
#include "cxx_parser_internal.h"

#include "cxx_debug.h"
#include "cxx_keyword.h"
#include "cxx_token.h"
#include "cxx_token_chain.h"
#include "cxx_scope.h"

#include "parse.h"
#include "vstring.h"
#include "get.h"
#include "debug.h"
#include "keyword.h"
#include "read.h"


#define MAX_NESTED_NAMESPACES 8


boolean cxxParserParseNamespace(void)
{
	CXX_DEBUG_ENTER();
	
	CXX_DEBUG_ASSERT(cxxParserCurrentLanguageIsCPP(),"This should be called only in C++");

	// FIXME: Do it better...

	cxxParserNewStatement(); // always a new statement

	/*
		Spec is:

			namespace ns_name { declarations }	(1)
			inline namespace ns_name { declarations }	(2)	(since C++11)
			namespace { declarations }	(3)
			namespace name = qualified-namespace ;	(7)
			namespace ns_name::name	{ (8)	(since C++17)
			
		Note that the using clauses have their own parsing routine and do not end up here.
	*/

	// namespace <X> {
	// namespace <X>::<Y>::<Z> {
	// namespace <X>::<Y>::<Z>;
	// namespace <X>;
	// namespace;

	unsigned int uProperties = 0;

	if(cxxTagFieldEnabled(CXXTagFieldProperties))
	{
		if(g_cxx.uKeywordState & CXXParserKeywordStateSeenInline)
			uProperties |= CXXTagPropertyInline;
	}

	int iScopeCount = 0;
	
	int i;

	int aCorkQueueIndices[MAX_NESTED_NAMESPACES];
	
	for(i=0;i<MAX_NESTED_NAMESPACES;i++)
		aCorkQueueIndices[i] = CORK_NIL;

	if(!cxxParserParseNextToken())
	{
		// syntax error, but we tolerate this
		CXX_DEBUG_LEAVE_TEXT("Implicit EOF in cxxParserParseNextToken");
		return TRUE; // EOF
	}

	if(cxxTokenTypeIs(g_cxx.pToken,CXXTokenTypeIdentifier))
	{
		// OK, check next token to see what's coming after

		CXX_DEBUG_PRINT("Got identifier %s",g_cxx.pToken->pszWord->buffer);

		CXXToken * pFirstIdentifier = g_cxx.pToken;
		CXXToken * pLastIdentifier = g_cxx.pToken;

		if(!cxxParserParseNextToken())
		{
			// syntax error, but we tolerate this
			CXX_DEBUG_LEAVE_TEXT("EOF in cxxParserParseNextToken");
			return TRUE; // EOF
		}
	
		switch(g_cxx.pToken->eType)
		{
			case CXXTokenTypeAssignment:
			{
				// probably namespace alias
				CXX_DEBUG_PRINT("Found assignment");
				
				if(!cxxParserParseNextToken())
				{
					// syntax error, but we tolerate this
					CXX_DEBUG_LEAVE_TEXT("EOF in cxxParserParseNextToken");
					return TRUE; // EOF
				}
				
				if(!cxxTokenTypeIsOneOf(
						g_cxx.pToken,
						CXXTokenTypeIdentifier | CXXTokenTypeMultipleColons
					))
				{
					CXX_DEBUG_LEAVE_TEXT("Some kind of syntax error here");
					return cxxParserSkipToSemicolonOrEOF();
				}

				CXXToken * pAlias = pFirstIdentifier;
				pFirstIdentifier = g_cxx.pToken;

				if(!cxxParserParseToEndOfQualifedName())
				{
					CXX_DEBUG_LEAVE_TEXT("Failed to parse the aliased name");
					return cxxParserSkipToSemicolonOrEOF();
				}

				pLastIdentifier = g_cxx.pToken->pPrev;

				tagEntryInfo * tag = cxxTagBegin(CXXTagCPPKindALIAS,pAlias);
	
				if(tag)
				{
					// This is highly questionable but well.. it's how old ctags did, so we do.
					tag->isFileScope = !isInputHeaderFile();

					CXXToken * pAliasedName = cxxTokenChainExtractRange(
							pFirstIdentifier,
							pLastIdentifier,
							CXXTokenChainExtractRangeNoTrailingSpaces
						);

					cxxTagSetField(
							CXXTagCPPFieldAliasedName,
							vStringValue(pAliasedName->pszWord)
						);

					cxxTagCommit();

					cxxTokenDestroy(pAliasedName);
				}

				CXX_DEBUG_LEAVE_TEXT("Finished parsing namespace alias");
				return cxxParserSkipToSemicolonOrEOF();
			}
			break;
			case CXXTokenTypeMultipleColons:
				// multi-namespace
				CXX_DEBUG_PRINT("Found multiple colons");

				if(!cxxParserParseToEndOfQualifedName())
				{
					CXX_DEBUG_LEAVE_TEXT("Failed to parse the namespace name");
					return cxxParserSkipToSemicolonOrEOF();
				}

				pLastIdentifier = g_cxx.pToken->pPrev;

				CXX_DEBUG_ASSERT(
						pFirstIdentifier != pLastIdentifier,
						"We expected multiple identifiers here"
					);

				if(!cxxTokenTypeIs(g_cxx.pToken,CXXTokenTypeOpeningBracket))
				{
					if(!cxxParserParseUpToOneOf(
							CXXTokenTypeOpeningBracket | CXXTokenTypeSemicolon | CXXTokenTypeEOF
						))
					{
						CXX_DEBUG_LEAVE_TEXT("Failed to parse up to an opening bracket");
						return FALSE;
					}

					if(!cxxTokenTypeIs(g_cxx.pToken,CXXTokenTypeOpeningBracket))
					{
						// tolerate syntax error
						CXX_DEBUG_LEAVE_TEXT("Found semicolon just after namespace declaration");
						return TRUE;
					}

					CXX_DEBUG_LEAVE_TEXT("Was expecting an opening bracket here");
					// FIXME: Maybe we could attempt to recover here?
					return TRUE;
				}
			break;
			case CXXTokenTypeOpeningBracket:
				// single name namespace
				CXX_DEBUG_PRINT("Found opening bracket");
			break;
			case CXXTokenTypeSemicolon:
				// tolerate syntax error
				CXX_DEBUG_LEAVE_TEXT("Found semicolon just after namespace declaration");
				return TRUE;
			break;
			default:
				CXX_DEBUG_LEAVE_TEXT("Some kind of syntax error here");
				return cxxParserSkipToSemicolonOrEOF();
			break;
		}

		CXX_DEBUG_ASSERT(
				cxxTokenTypeIs(g_cxx.pToken,CXXTokenTypeOpeningBracket),
				"Should have an opening bracket here!"
			);

		CXX_DEBUG_PRINT("Found regular namespace start");
		
		CXXToken * t = pFirstIdentifier;

		while(t)
		{
			tagEntryInfo * tag = cxxTagBegin(CXXTagCPPKindNAMESPACE,t);

			if(tag)
			{
				// This is highly questionable but well.. it's how old ctags did, so we do.
				tag->isFileScope = !isInputHeaderFile();

				vString * pszProperties = uProperties ? cxxTagSetProperties(uProperties) : NULL;

				int iCorkQueueIndex = cxxTagCommit();

				if(iScopeCount < MAX_NESTED_NAMESPACES)
					aCorkQueueIndices[iScopeCount] = iCorkQueueIndex;

				if(pszProperties)
					vStringDelete(pszProperties);
			}

			CXXToken * pNext = (t == pLastIdentifier) ? NULL : t->pNext->pNext;

			cxxTokenChainTake(g_cxx.pTokenChain,t);

			cxxScopePush(
					t,
					CXXScopeTypeNamespace,
					CXXScopeAccessUnknown
				);

			iScopeCount++;
			
			t = pNext;
		}

	} else if(cxxTokenTypeIs(g_cxx.pToken,CXXTokenTypeOpeningBracket))
	{
		// anonymous namespace
		CXX_DEBUG_PRINT("Found anonymous namespace start");

		CXXToken * t = cxxTokenCreateAnonymousIdentifier(CXXTagCPPKindNAMESPACE);
		tagEntryInfo * tag = cxxTagBegin(CXXTagCPPKindNAMESPACE,t);
		if(tag)
		{
			tag->isFileScope = !isInputHeaderFile();

			vString * pszProperties = uProperties ? cxxTagSetProperties(uProperties) : NULL;

			aCorkQueueIndices[0] = cxxTagCommit();

			if(pszProperties)
				vStringDelete(pszProperties);
		}
		cxxScopePush(t,CXXScopeTypeNamespace,CXXScopeAccessUnknown);

		iScopeCount++;

	} else {

		CXX_DEBUG_LEAVE_TEXT("Some kind of syntax error after namespace declaration");
		return cxxParserSkipToSemicolonOrEOF();
	}

	CXX_DEBUG_ASSERT(
			cxxTokenTypeIs(g_cxx.pToken,CXXTokenTypeOpeningBracket),
			"Should have an opening bracket here!"
		);

	// Here we certainly got an opening bracket: namespace block

	if(!cxxParserParseBlock(TRUE))
	{
		CXX_DEBUG_LEAVE_TEXT("Failed to parse namespace block");
		return FALSE;
	}

	while(iScopeCount > 0)
	{
		cxxScopePop();
		iScopeCount--;

		if(aCorkQueueIndices[iScopeCount] > CORK_NIL)
			cxxParserMarkEndLineForTagInCorkQueue(aCorkQueueIndices[iScopeCount]);
	}

	CXX_DEBUG_LEAVE_TEXT("Finished parsing namespace");
	return TRUE;
}
