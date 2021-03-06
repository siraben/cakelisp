#include "GeneratorHelpers.hpp"

#include "Evaluator.hpp"
#include "Tokenizer.hpp"
#include "Utilities.hpp"

// Generators receive the entire invocation. This function makes it easy to strip it away. It is
// useful to get the whole invocation in case the same generator is used with multiple different
// invocation strings
void StripInvocation(int& startTokenIndex, int& endTokenIndex)
{
	// ignore the "(blah"
	startTokenIndex += 2;
	// Ignore the final closing paren
	endTokenIndex -= 1;
}

// Note that the tokenizer should've already confirmed our parenthesis match, so we won't do
// validation here
int FindCloseParenTokenIndex(const std::vector<Token>& tokens, int startTokenIndex)
{
	if (tokens[startTokenIndex].type != TokenType_OpenParen)
		Log("Warning: FindCloseParenTokenIndex() expects to start on the opening parenthesis\n");

	int depth = 0;
	int numTokens = tokens.size();
	for (int i = startTokenIndex; i < numTokens; ++i)
	{
		const Token* token = &tokens[i];
		if (token->type == TokenType_OpenParen)
			depth++;
		else if (token->type == TokenType_CloseParen)
			depth--;

		if (depth == 0)
			return i;
	}

	return tokens.size();
}

bool ExpectEvaluatorScope(const char* generatorName, const Token& token,
                          const EvaluatorContext& context, EvaluatorScope expectedScope)
{
	if (context.scope != expectedScope)
	{
		ErrorAtTokenf(token, "%s expected to be invoked in %s scope, but is in %s scope",
		              generatorName, evaluatorScopeToString(expectedScope),
		              evaluatorScopeToString(context.scope));
		return false;
	}
	return true;
}

bool IsForbiddenEvaluatorScope(const char* generatorName, const Token& token,
                               const EvaluatorContext& context, EvaluatorScope forbiddenScope)
{
	if (context.scope == forbiddenScope)
	{
		ErrorAtTokenf(token, "%s cannot be invoked in %s scope", generatorName,
		              evaluatorScopeToString(forbiddenScope));
		return true;
	}
	return false;
}

bool ExpectTokenType(const char* generatorName, const Token& token, TokenType expectedType)
{
	if (token.type != expectedType)
	{
		ErrorAtTokenf(token, "%s expected %s, but got %s", generatorName,
		              tokenTypeToString(expectedType), tokenTypeToString(token.type));
		return false;
	}
	return true;
}

// Errors and returns false if out of invocation (or at closing paren)
bool ExpectInInvocation(const char* message, const std::vector<Token>& tokens, int indexToCheck,
                        int endInvocationIndex)
{
	if (indexToCheck >= endInvocationIndex)
	{
		const Token& endToken = tokens[endInvocationIndex];
		ErrorAtToken(endToken, message);
		return false;
	}
	return true;
}

// TODO: Come up with better name
bool isSpecialSymbol(const Token& token)
{
	if (token.type == TokenType_Symbol)
	{
		// The size check allows functions to be declared named ':' or '&', but not ':bad' or '&bad'
		return token.contents.size() > 1 &&
		       (token.contents[0] == ':' || token.contents[0] == '&' || token.contents[0] == '\'');
	}
	else
	{
		Log("Warning: isSpecialSymbol() expects only Symbol types\n");
		return true;
	}
}

// This function would be simpler and faster if there was an actual syntax tree, because we wouldn't
// be repeatedly traversing all the arguments
int getArgument(const std::vector<Token>& tokens, int startTokenIndex, int desiredArgumentIndex,
                int endTokenIndex)
{
	int currentArgumentIndex = 0;
	for (int i = startTokenIndex + 1; i < endTokenIndex; ++i)
	{
		if (currentArgumentIndex == desiredArgumentIndex)
			return i;

		const Token& token = tokens[i];
		if (token.type == TokenType_OpenParen)
		{
			// Skip any nesting
			i = FindCloseParenTokenIndex(tokens, i);
		}

		++currentArgumentIndex;
	}

	return -1;
}

int getExpectedArgument(const char* message, const std::vector<Token>& tokens, int startTokenIndex,
                        int desiredArgumentIndex, int endTokenIndex)
{
	int argumentIndex = getArgument(tokens, startTokenIndex, desiredArgumentIndex, endTokenIndex);

	if (argumentIndex == -1)
		ErrorAtTokenf(tokens[endTokenIndex], "missing arguments: %s", message);

	return argumentIndex;
}

int getNumArguments(const std::vector<Token>& tokens, int startTokenIndex, int endTokenIndex)
{
	int currentArgumentIndex = 0;
	for (int i = startTokenIndex + 1; i < endTokenIndex; ++i)
	{
		const Token& token = tokens[i];
		if (token.type == TokenType_OpenParen)
		{
			// Skip any nesting
			i = FindCloseParenTokenIndex(tokens, i);
		}

		++currentArgumentIndex;
	}
	return currentArgumentIndex;
}

bool ExpectNumArguments(const std::vector<Token>& tokens, int startTokenIndex, int endTokenIndex,
                        int numExpectedArguments)
{
	int numArguments = getNumArguments(tokens, startTokenIndex, endTokenIndex);
	if (numArguments != numExpectedArguments)
	{
		ErrorAtTokenf(tokens[startTokenIndex],
		              "expected %d arguments, got %d (counts include invocation as first argument)",
		              numExpectedArguments, numArguments);
		return false;
	}
	return true;
}

bool isLastArgument(const std::vector<Token>& tokens, int startTokenIndex, int endTokenIndex)
{
	if (tokens[startTokenIndex].type == TokenType_OpenParen &&
	    FindCloseParenTokenIndex(tokens, startTokenIndex) + 1 < endTokenIndex)
		return false;
	else if (tokens[startTokenIndex].type == TokenType_Symbol &&
	         startTokenIndex + 1 < endTokenIndex)
		return false;
	return true;
}

int getNextArgument(const std::vector<Token>& tokens, int currentTokenIndex, int endArrayTokenIndex)
{
	int nextArgStart = currentTokenIndex;
	if (tokens[currentTokenIndex].type == TokenType_OpenParen)
		nextArgStart = FindCloseParenTokenIndex(tokens, currentTokenIndex);

	++nextArgStart;
	return nextArgStart;
}

// If the current token is a scope, skip it. This is useful when a generator has already opened a
// block, so it knows the scope comes from the generator invocation
int blockAbsorbScope(const std::vector<Token>& tokens, int startBlockIndex)
{
	if (tokens[startBlockIndex].type == TokenType_OpenParen &&
	    (tokens[startBlockIndex + 1].contents.compare("scope") == 0 ||
	     tokens[startBlockIndex + 1].contents.compare("block") == 0))
		return startBlockIndex + 2;
	return startBlockIndex;
}

void MakeUniqueSymbolName(EvaluatorEnvironment& environment, const char* prefix,
                          Token* tokenToChange)
{
	char symbolNameBuffer[64] = {0};
	PrintfBuffer(symbolNameBuffer, "%s_%d", prefix, environment.nextFreeUniqueSymbolNum);

	tokenToChange->type = TokenType_Symbol;
	tokenToChange->contents = symbolNameBuffer;
	// TODO: If generated files are being checked in, it would be nice to have it be stable based on
	// file name or something
	environment.nextFreeUniqueSymbolNum++;
}

void MakeContextUniqueSymbolName(EvaluatorEnvironment& environment, const EvaluatorContext& context,
                                 const char* prefix, Token* tokenToChange)
{
	if (!context.definitionName)
	{
		MakeUniqueSymbolName(environment, prefix, tokenToChange);
		return;
	}

	ObjectDefinition* definition =
	    findObjectDefinition(environment, context.definitionName->contents.c_str());
	if (!definition)
	{
		MakeUniqueSymbolName(environment, prefix, tokenToChange);
		return;
	}

	char symbolNameBuffer[64] = {0};
	PrintfBuffer(symbolNameBuffer, "%s_%d", prefix, definition->nextFreeUniqueSymbolNum);

	tokenToChange->type = TokenType_Symbol;
	tokenToChange->contents = symbolNameBuffer;
	definition->nextFreeUniqueSymbolNum++;
}

const Token* FindTokenExpressionEnd(const Token* startToken)
{
	if (startToken->type != TokenType_OpenParen)
		return startToken;
	int depth = 0;
	for (const Token* currentToken = startToken; depth >= 0; ++currentToken)
	{
		if (currentToken->type == TokenType_OpenParen)
			++depth;
		else if (currentToken->type == TokenType_CloseParen)
		{
			--depth;
			if (depth <= 0)
				return currentToken;
		}
	}
	return nullptr;
}

static void CopyTokensWithMacrosExpanded_Recursive(const Token* startToken, const Token* endToken,
                                                   const std::vector<MacroExpansion>& expansions,
                                                   std::vector<Token>& tokensOut)
{
	for (const Token* currentToken = startToken; currentToken <= endToken;)
	{
		// TODO: Performance: O(nm)
		bool macroFound = false;
		for (const MacroExpansion& expansion : expansions)
		{
			if (currentToken == expansion.atToken)
			{
				unsigned int numTokensInExpansion = expansion.tokens->size();
				tokensOut.reserve(tokensOut.size() + numTokensInExpansion);
				CopyTokensWithMacrosExpanded_Recursive(
				    &(*expansion.tokens)[0], &(*expansion.tokens)[numTokensInExpansion - 1],
				    expansions, tokensOut);

				macroFound = true;
				break;
			}
		}

		if (macroFound)
		{
			// Skip the macro invocation; we've already replaced it with the expansion
			currentToken = FindTokenExpressionEnd(currentToken);
			++currentToken;
		}
		else
		{
			tokensOut.push_back(*currentToken);
			++currentToken;
		}
	}
}

bool CreateDefinitionCopyMacroExpanded(const ObjectDefinition& definition,
                                       std::vector<Token>& tokensOut)
{
	if (!(definition.type == ObjectType_Function || definition.type == ObjectType_Variable))
	{
		Logf(
		    "error: CreateDefinitionCopyMacroExpanded() called on definition type %s which is not "
		    "explicitly supported by this function. Check CreateDefinitionCopyMacroExpanded() and "
		    "ensure your type's signature is understood, and that your type is tracking its macro "
		    "expansions, then add it as a supported type\n",
		    objectTypeToString(definition.type));
		return false;
	}

	if (!definition.definitionInvocation)
	{
		Log("error: CreateDefinitionCopyMacroExpanded() called on definition which did not set "
		    "invocation token. This is necessary to accurately copy the definition\n");
		return false;
	}

	if (definition.type == ObjectType_Function || definition.type == ObjectType_Variable)
	{
		// TODO: Performance: Extra traversal over entire definition
		const Token* endToken = FindTokenExpressionEnd(definition.definitionInvocation);

		// It may be a bit larger or smaller depending on whether macros output more or less tokens
		tokensOut.reserve((endToken - definition.definitionInvocation) + 1);

		CopyTokensWithMacrosExpanded_Recursive(definition.definitionInvocation, endToken,
		                                       definition.macroExpansions, tokensOut);
	}

	return true;
}

//
// Token list manipulation
//

void PushBackTokenExpression(std::vector<Token>& output, const Token* startToken)
{
	if (!startToken)
	{
		Log("error: PushBackTokenExpression() received null token\n");
		return;
	}

	if (startToken->type != TokenType_OpenParen)
	{
		output.push_back(*startToken);
	}
	else
	{
		int depth = 0;
		for (const Token* currentToken = startToken; depth >= 0; ++currentToken)
		{
			if (currentToken->type == TokenType_OpenParen)
				++depth;
			else if (currentToken->type == TokenType_CloseParen)
				--depth;

			output.push_back(*currentToken);

			if (depth == 0)
				break;
		}
	}
}

//
// Outputting
//

void addModifierToStringOutput(StringOutput& operation, StringOutputModifierFlags flag)
{
	operation.modifiers = (StringOutputModifierFlags)((int)operation.modifiers | (int)flag);
}

void addStringOutput(std::vector<StringOutput>& output, const std::string& symbol,
                     StringOutputModifierFlags modifiers, const Token* startToken)
{
	StringOutput newStringOutput = {};
	newStringOutput.modifiers = modifiers;
	newStringOutput.startToken = startToken;

	newStringOutput.output = symbol;

	output.push_back(std::move(newStringOutput));
}

void addLangTokenOutput(std::vector<StringOutput>& output, StringOutputModifierFlags modifiers,
                        const Token* startToken)
{
	StringOutput newStringOutput = {};
	newStringOutput.modifiers = modifiers;
	newStringOutput.startToken = startToken;

	output.push_back(std::move(newStringOutput));
}

void addSpliceOutput(GeneratorOutput& output, GeneratorOutput* spliceOutput,
                     const Token* startToken)
{
	StringOutput newStringOutput = {};
	// No other modifiers are valid because splice is handled outside the normal writer
	newStringOutput.modifiers = StringOutMod_Splice;
	newStringOutput.startToken = startToken;

	newStringOutput.spliceOutput = spliceOutput;

	// Splice marker must be pushed to both source and header to preserve ordering in case
	// spliceOutput has both source and header outputs
	output.source.push_back(std::move(newStringOutput));
	output.header.push_back(std::move(newStringOutput));
}

//
// Function signatures
//

bool parseFunctionSignature(const std::vector<Token>& tokens, int argsIndex,
                            std::vector<FunctionArgumentTokens>& arguments, int& returnTypeStart)
{
	enum DefunState
	{
		Name,
		Type,
		ReturnType
	};

	DefunState state = Name;
	FunctionArgumentTokens currentArgument = {};

	int endArgsIndex = FindCloseParenTokenIndex(tokens, argsIndex);
	for (int i = argsIndex + 1; i < endArgsIndex; ++i)
	{
		const Token& currentToken = tokens[i];

		if (state == ReturnType)
		{
			returnTypeStart = i;
			break;
		}
		else if (state == Name)
		{
			if (currentToken.type == TokenType_Symbol &&
			    currentToken.contents.compare("&return") == 0)
			{
				state = ReturnType;
				if (!ExpectInInvocation("&return expected type", tokens, i + 1, endArgsIndex))
					return false;
				// Wait until next token to get type
				continue;
			}

			if (!ExpectTokenType("defun", currentToken, TokenType_Symbol))
				return false;

			currentArgument.nameIndex = i;
			state = Type;

			// We've now introduced an expectation that a name will follow
			if (!ExpectInInvocation("expected argument type", tokens, i + 1, endArgsIndex))
				return false;
		}
		else if (state == Type)
		{
			if (currentToken.type == TokenType_Symbol && isSpecialSymbol(currentToken))
			{
				ErrorAtTokenf(currentToken,
				              "defun expected argument type, but got symbol or marker %s",
				              currentToken.contents.c_str());
				return false;
			}

			if (currentToken.type != TokenType_OpenParen && currentToken.type != TokenType_Symbol)
			{
				ErrorAtTokenf(currentToken, "defun expected argument type, got %s",
				              tokenTypeToString(currentToken.type));
				return false;
			}

			currentArgument.startTypeIndex = i;

			// Finished with an argument
			arguments.push_back(currentArgument);
			currentArgument = {};

			state = Name;
			// Skip past type declaration; it will be handled later
			if (currentToken.type == TokenType_OpenParen)
			{
				i = FindCloseParenTokenIndex(tokens, i);
			}
		}
	}

	return true;
}

// startInvocationIndex is used for blaming on implicit return type
bool outputFunctionReturnType(const std::vector<Token>& tokens, GeneratorOutput& output,
                              int returnTypeStart, int startInvocationIndex, int endArgsIndex,
                              bool outputSource, bool outputHeader)
{
	if (returnTypeStart == -1)
	{
		// The type was implicit; blame the "defun"
		if (outputSource)
			addStringOutput(output.source, "void", StringOutMod_SpaceAfter,
			                &tokens[startInvocationIndex]);
		if (outputHeader)
			addStringOutput(output.header, "void", StringOutMod_SpaceAfter,
			                &tokens[startInvocationIndex]);
	}
	else
	{
		const Token& returnTypeToken = tokens[returnTypeStart];

		// Check whether any arguments followed return type, because they will be ignored
		{
			int returnTypeEndIndex = returnTypeStart;
			if (returnTypeToken.type == TokenType_OpenParen)
				returnTypeEndIndex = FindCloseParenTokenIndex(tokens, returnTypeStart);

			if (returnTypeEndIndex + 1 < endArgsIndex)
			{
				const Token& extraneousToken = tokens[returnTypeEndIndex + 1];
				ErrorAtToken(extraneousToken, "Arguments after &return type are ignored");
				return false;
			}
		}

		std::vector<StringOutput> typeOutput;
		std::vector<StringOutput> afterNameOutput;
		// Arrays cannot be return types, they must be * instead
		if (!tokenizedCTypeToString_Recursive(tokens, returnTypeStart,
		                                      /*allowArray=*/false, typeOutput, afterNameOutput))
			return false;

		if (!afterNameOutput.empty())
		{
			const Token* problemToken = afterNameOutput.begin()->startToken;
			ErrorAtToken(*problemToken,
			             "Return types cannot have this type. An error in the code has occurred, "
			             "because the parser shouldn't have gotten this far");
			return false;
		}

		// Functions need a space between type and name; add it
		addModifierToStringOutput(typeOutput.back(), StringOutMod_SpaceAfter);

		if (outputSource)
			output.source.insert(output.source.end(), typeOutput.begin(), typeOutput.end());
		if (outputHeader)
			output.header.insert(output.header.end(), typeOutput.begin(), typeOutput.end());
	}

	return true;
}

bool outputFunctionArguments(const std::vector<Token>& tokens, GeneratorOutput& output,
                             const std::vector<FunctionArgumentTokens>& arguments,
                             bool outputSource, bool outputHeader)
{
	int numFunctionArguments = arguments.size();
	for (int i = 0; i < numFunctionArguments; ++i)
	{
		const FunctionArgumentTokens& arg = arguments[i];
		std::vector<StringOutput> typeOutput;
		std::vector<StringOutput> afterNameOutput;
		bool typeValid =
		    tokenizedCTypeToString_Recursive(tokens, arg.startTypeIndex,
		                                     /*allowArray=*/true, typeOutput, afterNameOutput);
		if (!typeValid)
			return false;

		addModifierToStringOutput(typeOutput.back(), StringOutMod_SpaceAfter);

		// Type
		if (outputSource)
			PushBackAll(output.source, typeOutput);
		if (outputHeader)
			PushBackAll(output.header, typeOutput);

		// Name
		if (outputSource)
			addStringOutput(output.source, tokens[arg.nameIndex].contents,
			                StringOutMod_ConvertVariableName, &tokens[arg.nameIndex]);
		if (outputHeader)
			addStringOutput(output.header, tokens[arg.nameIndex].contents,
			                StringOutMod_ConvertVariableName, &tokens[arg.nameIndex]);

		// Array
		if (outputSource)
			PushBackAll(output.source, afterNameOutput);
		if (outputHeader)
			PushBackAll(output.header, afterNameOutput);

		if (i + 1 < numFunctionArguments)
		{
			if (outputSource)
				addLangTokenOutput(output.source, StringOutMod_ListSeparator,
				                   &tokens[arg.nameIndex]);
			if (outputHeader)
				addLangTokenOutput(output.header, StringOutMod_ListSeparator,
				                   &tokens[arg.nameIndex]);
		}
	}

	return true;
}

// afterNameOutput must be a separate buffer because some C type specifiers (e.g. array []) need to
// come after the type. Returns whether parsing was successful
bool tokenizedCTypeToString_Recursive(const std::vector<Token>& tokens, int startTokenIndex,
                                      bool allowArray, std::vector<StringOutput>& typeOutput,
                                      std::vector<StringOutput>& afterNameOutput)
{
	if (&typeOutput == &afterNameOutput)
	{
		Log("Error: tokenizedCTypeToString_Recursive() requires a separate output buffer for "
		    "after-name types\n");
		return false;
	}

	// A type name
	if (tokens[startTokenIndex].type == TokenType_Symbol)
	{
		if (isSpecialSymbol(tokens[startTokenIndex]))
		{
			ErrorAtToken(tokens[startTokenIndex],
			             "types must not be : keywords or & sentinels. A generator may be "
			             "misinterpreting the special symbol, or you have made a mistake");
			return false;
		}

		addStringOutput(typeOutput, tokens[startTokenIndex].contents, StringOutMod_ConvertTypeName,
		                &tokens[startTokenIndex]);

		return true;
	}
	else
	{
		// Some examples:
		// (const int)
		// (* (const char))
		// (& (const (<> std::vector Token)))
		// ([] (const char))
		// ([] ([] 10 float)) ;; 2D Array with one specified dimension

		const Token& typeInvocation = tokens[startTokenIndex + 1];
		if (!ExpectTokenType("C/C++ type parser generator", typeInvocation, TokenType_Symbol))
			return false;

		int endTokenIndex = FindCloseParenTokenIndex(tokens, startTokenIndex);

		if (typeInvocation.contents.compare("const") == 0)
		{
			if (!ExpectNumArguments(tokens, startTokenIndex, endTokenIndex, 2))
				return false;

			// Prepend const-ness
			addStringOutput(typeOutput, "const", StringOutMod_SpaceAfter, &typeInvocation);

			int typeIndex = getExpectedArgument("const requires type", tokens, startTokenIndex, 1,
			                                    endTokenIndex);
			if (typeIndex == -1)
				return false;

			return tokenizedCTypeToString_Recursive(tokens, typeIndex, allowArray, typeOutput,
			                                        afterNameOutput);
		}
		else if (typeInvocation.contents.compare("*") == 0 ||
		         typeInvocation.contents.compare("&") == 0)
		{
			if (!ExpectNumArguments(tokens, startTokenIndex, endTokenIndex, 2))
				return false;

			// Append pointer/reference
			int typeIndex =
			    getExpectedArgument("expected type", tokens, startTokenIndex, 1, endTokenIndex);
			if (typeIndex == -1)
				return false;

			if (!tokenizedCTypeToString_Recursive(tokens, typeIndex, allowArray, typeOutput,
			                                      afterNameOutput))
				return false;

			addStringOutput(typeOutput, typeInvocation.contents.c_str(), StringOutMod_None,
			                &typeInvocation);
		}
		else if (typeInvocation.contents.compare("&&") == 0 ||
		         typeInvocation.contents.compare("rval-ref-to") == 0)
		{
			if (!ExpectNumArguments(tokens, startTokenIndex, endTokenIndex, 2))
				return false;

			int typeIndex =
			    getExpectedArgument("expected type", tokens, startTokenIndex, 1, endTokenIndex);
			if (typeIndex == -1)
				return false;

			if (!tokenizedCTypeToString_Recursive(tokens, typeIndex, allowArray, typeOutput,
			                                      afterNameOutput))
				return false;

			addStringOutput(typeOutput, "&&", StringOutMod_None, &typeInvocation);
		}
		else if (typeInvocation.contents.compare("<>") == 0)
		{
			int typeIndex = getExpectedArgument("expected template name", tokens, startTokenIndex,
			                                    1, endTokenIndex);
			if (typeIndex == -1)
				return false;

			if (!tokenizedCTypeToString_Recursive(tokens, typeIndex, allowArray, typeOutput,
			                                      afterNameOutput))
				return false;

			addStringOutput(typeOutput, "<", StringOutMod_None, &typeInvocation);
			for (int startTemplateParameter = typeIndex + 1; startTemplateParameter < endTokenIndex;
			     ++startTemplateParameter)
			{
				// Override allowArray for subsequent parsing, because otherwise, the array args
				// will be appended to the wrong buffer, and you cannot declare arrays in template
				// parameters anyways (as far as I can tell)
				if (!tokenizedCTypeToString_Recursive(tokens, startTemplateParameter,
				                                      /*allowArray=*/false, typeOutput,
				                                      afterNameOutput))
					return false;

				if (!isLastArgument(tokens, startTemplateParameter, endTokenIndex))
					addLangTokenOutput(typeOutput, StringOutMod_ListSeparator,
					                   &tokens[startTemplateParameter]);

				// Skip over tokens of the type we just parsed (the for loop increment will move us
				// off the end paren)
				if (tokens[startTemplateParameter].type == TokenType_OpenParen)
					startTemplateParameter =
					    FindCloseParenTokenIndex(tokens, startTemplateParameter);
			}
			addStringOutput(typeOutput, ">", StringOutMod_None, &typeInvocation);
		}
		else if (typeInvocation.contents.compare("[]") == 0)
		{
			if (!allowArray)
			{
				ErrorAtToken(
				    tokens[startTokenIndex],
				    "cannot declare array in this context. You may need to use a pointer instead");
				return false;
			}

			int firstArgIndex = getExpectedArgument("expected type or array size", tokens,
			                                        startTokenIndex, 1, endTokenIndex);
			if (firstArgIndex == -1)
				return false;

			// Arrays must append their brackets after the name (must be in separate buffer)
			bool arraySizeIsFirstArgument = tokens[firstArgIndex].type == TokenType_Symbol &&
			                                std::isdigit(tokens[firstArgIndex].contents[0]);
			int typeIndex = firstArgIndex;
			if (arraySizeIsFirstArgument)
			{
				typeIndex = getExpectedArgument("expected array type", tokens, startTokenIndex, 2,
				                                endTokenIndex);
				if (typeIndex == -1)
					return false;

				// Array size specified as first argument
				addStringOutput(afterNameOutput, "[", StringOutMod_None, &typeInvocation);
				addStringOutput(afterNameOutput, tokens[firstArgIndex].contents.c_str(),
				                StringOutMod_None, &tokens[firstArgIndex]);
				addStringOutput(afterNameOutput, "]", StringOutMod_None, &typeInvocation);
			}
			else
				addStringOutput(afterNameOutput, "[]", StringOutMod_None, &typeInvocation);

			// Type parsing happens after the [] have already been appended because the array's type
			// may include another array dimension, which must be specified after the current array
			return tokenizedCTypeToString_Recursive(tokens, typeIndex,
			                                        /*allowArray=*/true, typeOutput,
			                                        afterNameOutput);
		}
		// else if (typeInvocation.contents.compare("::") == 0)
		else if (typeInvocation.contents.compare("in") == 0)
		{
			int firstScopeIndex =
			    getExpectedArgument("expected scope", tokens, startTokenIndex, 1, endTokenIndex);
			if (firstScopeIndex == -1)
				return false;

			for (int startScopeIndex = firstScopeIndex; startScopeIndex < endTokenIndex;
			     startScopeIndex = getNextArgument(tokens, startScopeIndex, endTokenIndex))
			{
				// Override allowArray for subsequent parsing, because otherwise, the array args
				// will be appended to the wrong buffer, and you cannot declare arrays in scope
				// parameters anyways (as far as I can tell)
				if (!tokenizedCTypeToString_Recursive(tokens, startScopeIndex,
				                                      /*allowArray=*/false, typeOutput,
				                                      afterNameOutput))
					return false;

				if (!isLastArgument(tokens, startScopeIndex, endTokenIndex))
					addStringOutput(typeOutput, "::", StringOutMod_None, &tokens[startScopeIndex]);
			}
		}
		else
		{
			ErrorAtToken(typeInvocation, "unknown C/C++ type specifier");
			return false;
		}
		return true;
	}
}

bool CompileTimeFunctionSignatureMatches(EvaluatorEnvironment& environment, const Token& errorToken,
                                         const char* compileTimeFunctionName,
                                         const std::vector<Token>& expectedSignature)
{
	CompileTimeFunctionMetadataTableIterator findIt =
	    environment.compileTimeFunctionInfo.find(compileTimeFunctionName);
	if (findIt == environment.compileTimeFunctionInfo.end())
	{
		ErrorAtToken(errorToken,
		             "could not find function metadata to validate signature. Internal "
		             "code error");
		return false;
	}
	CompileTimeFunctionMetadata& functionMetadata = findIt->second;
	const Token* endUserArgs = FindTokenExpressionEnd(functionMetadata.startArgsToken);
	const Token* currentUserArgToken = functionMetadata.startArgsToken;
	int numArgumentsProvided = (endUserArgs - currentUserArgToken) + 1;
	if (numArgumentsProvided != (long)expectedSignature.size())
	{
		ErrorAtToken(*functionMetadata.startArgsToken,
		             "arguments do not match expected function signature "
		             "printed below (' = name does not need to match):");
		printTokens(expectedSignature);

		Logf("too many/few tokens. %i need %lu\n", numArgumentsProvided, expectedSignature.size());
		return false;
	}
	for (unsigned int i = 0; i < expectedSignature.size() && currentUserArgToken != endUserArgs;
	     ++currentUserArgToken, ++i)
	{
		const Token* expectedToken = &(expectedSignature[i]);
		// Ignore variable names
		if (expectedToken->type == TokenType_Symbol && expectedToken->contents[0] == '\'')
			continue;

		if (expectedToken->type != currentUserArgToken->type ||
		    expectedToken->contents.compare(currentUserArgToken->contents) != 0)
		{
			ErrorAtToken(*currentUserArgToken,
			             "arguments do not match expected function signature "
			             "printed below (' = name does not need to match):");
			printTokens(expectedSignature);
			return false;
		}
	}

	return true;
}

//
// C Statement generation
//

bool CStatementOutput(EvaluatorEnvironment& environment, const EvaluatorContext& context,
                      const std::vector<Token>& tokens, int startTokenIndex,
                      const CStatementOperation* operation, int numOperations,
                      GeneratorOutput& output)
{
	// TODO: Add expects for scope
	int endTokenIndex = FindCloseParenTokenIndex(tokens, startTokenIndex);
	int nameTokenIndex = startTokenIndex + 1;
	// int startArgsIndex = nameTokenIndex + 1;
	const Token& nameToken = tokens[nameTokenIndex];
	for (int i = 0; i < numOperations; ++i)
	{
		switch (operation[i].type)
		{
			case Keyword:
				addStringOutput(output.source, operation[i].keywordOrSymbol,
				                StringOutMod_SpaceAfter, &nameToken);
				break;
			case KeywordNoSpace:
				addStringOutput(output.source, operation[i].keywordOrSymbol, StringOutMod_None,
				                &nameToken);
				break;
			case SpliceNoSpace:
			case Splice:
			{
				if (operation[i].argumentIndex < 0)
				{
					Log("Error: Expected valid argument index for start of splice list\n");
					return false;
				}
				int startSpliceListIndex =
				    getExpectedArgument("expected expressions", tokens, startTokenIndex,
				                        operation[i].argumentIndex, endTokenIndex);
				if (startSpliceListIndex == -1)
					return false;
				EvaluatorContext bodyContext = context;
				bodyContext.scope = EvaluatorScope_ExpressionsOnly;
				StringOutput spliceDelimiterTemplate = {};
				spliceDelimiterTemplate.output = operation[i].keywordOrSymbol;
				addModifierToStringOutput(spliceDelimiterTemplate, StringOutMod_SpaceBefore);
				addModifierToStringOutput(spliceDelimiterTemplate, StringOutMod_SpaceAfter);
				bodyContext.delimiterTemplate = spliceDelimiterTemplate;
				int numErrors = EvaluateGenerateAll_Recursive(environment, bodyContext, tokens,
				                                              startSpliceListIndex, output);
				if (numErrors)
					return false;
				break;
			}
			case OpenParen:
				addLangTokenOutput(output.source, StringOutMod_OpenParen, &nameToken);
				break;
			case CloseParen:
				addLangTokenOutput(output.source, StringOutMod_CloseParen, &nameToken);
				break;
			case OpenBlock:
				addLangTokenOutput(output.source, StringOutMod_OpenBlock, &nameToken);
				break;
			case CloseBlock:
				addLangTokenOutput(output.source, StringOutMod_CloseBlock, &nameToken);
				break;
			case OpenList:
				addLangTokenOutput(output.source, StringOutMod_OpenList, &nameToken);
				break;
			case CloseList:
				addLangTokenOutput(output.source, StringOutMod_CloseList, &nameToken);
				break;
			case SmartEndStatement:
				if (context.scope != EvaluatorScope_ExpressionsOnly)
					addLangTokenOutput(output.source, StringOutMod_EndStatement, &nameToken);
				break;
			case TypeNoArray:
			{
				if (operation[i].argumentIndex < 0)
				{
					Log("Error: Expected valid argument index for expression\n");
					return false;
				}
				int startTypeIndex = getExpectedArgument("expected type", tokens, startTokenIndex,
				                                         operation[i].argumentIndex, endTokenIndex);
				if (startTypeIndex == -1)
					return false;
				std::vector<StringOutput> typeOutput;
				std::vector<StringOutput> typeAfterNameOutput;
				if (!tokenizedCTypeToString_Recursive(tokens, startTypeIndex,
				                                      /*allowArray=*/false, typeOutput,
				                                      typeAfterNameOutput))
					return false;

				PushBackAll(output.source, typeOutput);
				break;
			}
			case ExpressionOptional:
			{
				if (operation[i].argumentIndex < 0)
				{
					Log("Error: Expected valid argument index for expression\n");
					return false;
				}
				int startExpressionIndex =
				    getArgument(tokens, startTokenIndex, operation[i].argumentIndex, endTokenIndex);
				if (startExpressionIndex == -1)
				{
					// Fine by us, it's optional
					break;
				}
				EvaluatorContext expressionContext = context;
				expressionContext.scope = EvaluatorScope_ExpressionsOnly;
				if (EvaluateGenerate_Recursive(environment, expressionContext, tokens,
				                               startExpressionIndex, output) != 0)
					return false;
				break;
			}
			case Expression:
			{
				if (operation[i].argumentIndex < 0)
				{
					Log("Error: Expected valid argument index for expression\n");
					return false;
				}
				int startExpressionIndex =
				    getExpectedArgument("expected expression", tokens, startTokenIndex,
				                        operation[i].argumentIndex, endTokenIndex);
				if (startExpressionIndex == -1)
					return false;
				EvaluatorContext expressionContext = context;
				expressionContext.scope = EvaluatorScope_ExpressionsOnly;
				if (EvaluateGenerate_Recursive(environment, expressionContext, tokens,
				                               startExpressionIndex, output) != 0)
					return false;
				break;
			}
			case ExpressionList:
			{
				if (operation[i].argumentIndex < 0)
				{
					Log("Error: Expected valid argument index for expression\n");
					return false;
				}
				// We're actually fine with no arguments
				int startExpressionIndex =
				    getArgument(tokens, startTokenIndex, operation[i].argumentIndex, endTokenIndex);
				if (startExpressionIndex == -1)
					break;
				EvaluatorContext expressionContext = context;
				expressionContext.scope = EvaluatorScope_ExpressionsOnly;
				StringOutput listDelimiterTemplate = {};
				listDelimiterTemplate.modifiers = StringOutMod_ListSeparator;
				expressionContext.delimiterTemplate = listDelimiterTemplate;

				if (EvaluateGenerateAll_Recursive(environment, expressionContext, tokens,
				                                  startExpressionIndex, output) != 0)
					return false;
				break;
			}
			case Body:
			{
				if (operation[i].argumentIndex < 0)
				{
					Log("Error: Expected valid argument index for body\n");
					return false;
				}
				int startBodyIndex = getExpectedArgument("expected body", tokens, startTokenIndex,
				                                         operation[i].argumentIndex, endTokenIndex);
				if (startBodyIndex == -1)
					return false;
				EvaluatorContext bodyContext = context;
				bodyContext.scope = EvaluatorScope_Body;
				// The statements will need to handle their ;
				bodyContext.delimiterTemplate = {};
				int numErrors = EvaluateGenerateAll_Recursive(environment, bodyContext, tokens,
				                                              startBodyIndex, output);
				if (numErrors)
					return false;
				break;
			}
			default:
				Log("Output type not handled\n");
				return false;
		}
	}

	return true;
}
