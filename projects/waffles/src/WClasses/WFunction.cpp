/*
  The contents of this file are dedicated by all of its authors, including

    Michael S. Gashler,
    Eric Moyer,
    anonymous contributors,

  to the public domain (http://creativecommons.org/publicdomain/zero/1.0/).

  Note that some moral obligations still exist in the absence of legal ones.
  For example, it would still be dishonest to deliberately misrepresent the
  origin of a work. Although we impose no legal requirements to obtain a
  license, it is beseeming for those who build on the works of others to
  give back useful improvements, or find a way to pay it forward. If
  you would like to cite us, a published paper about Waffles can be found
  at http://jmlr.org/papers/volume12/gashler11a/gashler11a.pdf. If you find
  our code to be useful, the Waffles team would love to hear how you use it.
*/

#include "WFunction.h"
#include <math.h>
#include "WError.h"
#include "WHolders.h"
#include "WMath.h"
#include <stdlib.h>
#include <cmath>
#include <string>
#include <memory>
#include <utility>

using namespace WClasses;
using std::vector;
using std::string;
using std::map;

typedef double (*MathFuncFunc)(vector<double>& params);

namespace WClasses
{
class WFunctionNode
{
public:
	WFunctionNode()
	{
	}

	virtual ~WFunctionNode()
	{
	}

	virtual double eval(vector<double>& params, WFunctionParser& parser) = 0;
	virtual void unlink(const char* szName) = 0;
};
}


WFunction::WFunction(WFunctionNode* pRoot, int expectedParams) : m_pRoot(pRoot), m_expectedParams(expectedParams)
{
}

WFunction::~WFunction()
{
	delete(m_pRoot);
}

double WFunction::call(std::vector<double>& params, WFunctionParser& parser)
{
	return m_pRoot->eval(params, parser);
}


namespace WClasses
{
class WFunctionBuiltIn : public WFunctionNode
{
public:
	MathFuncFunc m_pFunc;

	WFunctionBuiltIn(MathFuncFunc pFunc) : WFunctionNode(), m_pFunc(pFunc)
	{
	}

	~WFunctionBuiltIn() override
	{
	}

	double eval(vector<double>& params, WFunctionParser& parser) override
	{
		return m_pFunc(params);
	}

	void unlink(const char* szName) override {}

	// The operators
	static double plus(vector<double>& params) { return params[0] + params[1]; }
	static double minus(vector<double>& params) { return params[0] - params[1]; }
	static double times(vector<double>& params) { return params[0] * params[1]; }
	static double divide(vector<double>& params) { return params[0] / params[1]; }
	static double modulus(vector<double>& params) { return fmod(params[0], params[1]); }
	static double exponent(vector<double>& params) { return pow(params[0], params[1]); }
	static double negate(vector<double>& params) { return -params[0]; }

	// built-in functions
	static double _abs(vector<double>& params) { return std::abs(params[0]); }
	static double _acos(vector<double>& params) { return acos(params[0]); }
#ifndef WINDOWS
	static double _acosh(vector<double>& params) { return acosh(params[0]); }
#endif
	static double _asin(vector<double>& params) { return asin(params[0]); }
#ifndef WINDOWS
	static double _asinh(vector<double>& params) { return asinh(params[0]); }
#endif
	static double _atan(vector<double>& params) { return atan(params[0]); }
#ifndef WINDOWS
	static double _atanh(vector<double>& params) { return atanh(params[0]); }
#endif
	static double _ceil(vector<double>& params) { return ceil(params[0]); }
	static double _cos(vector<double>& params) { return cos(params[0]); }
	static double _cosh(vector<double>& params) { return cosh(params[0]); }
#ifndef WINDOWS
	static double _erf(vector<double>& params) { return erf(params[0]); }
#endif
	static double _floor(vector<double>& params) { return floor(params[0]); }
	static double _gamma(vector<double>& params) { return WMath::gamma(params[0]); }
	static double _ifzero(vector<double>& params) { return (std::abs(params[0]) < 1e-12) ? params[1] : params[2]; }
	static double _ifnegative(vector<double>& params) { return (params[0] < 0) ? params[1] : params[2]; }
	static double _lgamma(vector<double>& params) { return WMath::logGamma(params[0]); }
	static double _log(vector<double>& params) { return log(params[0]); }
	static double _softexp(vector<double>& params) { return WMath::softExponential(params[0], params[1]); }
	static double _max(vector<double>& params)
	{
		vector<double>::iterator it = params.begin();
		double d = *it;
		for(it++; it != params.end(); it++)
			d = std::max(d, *it);
		return d;
	}
	static double _min(vector<double>& params)
	{
		vector<double>::iterator it = params.begin();
		double d = *it;
		for(it++; it != params.end(); it++)
			d = std::min(d, *it);
		return d;
	}
	static double _normal(vector<double>& params) { return 0.39894228 * exp(-0.5 * params[0] * params[0]); }
	static double _sign(vector<double>& params) { return params[0] >= 0 ? 1.0 : -1.0; }
	static double _sin(vector<double>& params) { return sin(params[0]); }
	static double _sinh(vector<double>& params) { return sinh(params[0]); }
	static double _sqrt(vector<double>& params) { return sqrt(params[0]); }
	static double _tan(vector<double>& params) { return tan(params[0]); }
	static double _tanh(vector<double>& params) { return tanh(params[0]); }
};

class WFunctionCall : public WFunctionNode
{
private:
	string m_name;
	WFunctionNode* m_pFunction;
	vector<WFunctionNode*> m_children;
	vector<double> m_params; // a buffer to hold the result from evaluating each parameter expression, then used to call this function

public:
	WFunctionCall(WFunctionNode* pFunction) : WFunctionNode(), m_pFunction(pFunction)
	{
	}

	WFunctionCall(string name) : WFunctionNode(), m_name(std::move(name)), m_pFunction(nullptr)
	{
	}

	~WFunctionCall() override
	{
		for(vector<WFunctionNode*>::iterator it = m_children.begin(); it != m_children.end(); it++)
			delete(*it);
	}

	void AddChild(WFunctionNode* pChild)
	{
		m_children.push_back(pChild);
		m_params.push_back(0.0);
	}

	double eval(vector<double>& params, WFunctionParser& parser) override
	{
		for(size_t i = 0; i < m_children.size(); i++)
			m_params[i] = m_children[i]->eval(params, parser); // "params" holds the parameters to the root function. They are used when a leaf turns out to be one of the variables of the root function.
		if(!m_pFunction)
		{
			WFunction* pFunc = parser.getFunction(m_name.c_str());
			if(pFunc->m_expectedParams >= 0)
			{
				// An exact number of parameters is expected
				if((int)m_children.size() != pFunc->m_expectedParams)
					throw Ex("The function ", m_name, " expects ", to_str(pFunc->m_expectedParams), " parameters. (Trying to call it with ", to_str(m_children.size()), ").");
			}
			else
			{
				// A minimum number of parameters is expected
				if((int)m_children.size() < -pFunc->m_expectedParams - 1)
					throw Ex("The function ", m_name, " expects at least ", to_str(-pFunc->m_expectedParams - 1), " parameters. (Trying to call it with ", to_str(m_children.size()), ".)");
			}
			m_pFunction = pFunc->m_pRoot;
		}
		return m_pFunction->eval(m_params, parser);
	}

	void unlink(const char* szName) override
	{
		for(vector<WFunctionNode*>::iterator it = m_children.begin(); it != m_children.end(); it++)
			(*it)->unlink(szName);
		if(m_name.compare(szName) == 0)
			m_pFunction = nullptr;
	}
};

class WFunctionVariable : public WFunctionNode
{
public:
	int m_index;

	WFunctionVariable(int index) : WFunctionNode(), m_index(index)
	{
	}

	~WFunctionVariable() override
	{
	}

	double eval(vector<double>& params, WFunctionParser& parser) override
	{
		return params[m_index];
	}

	void unlink(const char* szName) override {}
};

class WFunctionConstant : public WFunctionNode
{
public:
	double m_value;

	WFunctionConstant(double value) : WFunctionNode(), m_value(value)
	{
	}

	~WFunctionConstant() override
	{
	}

	double eval(vector<double>& params, WFunctionParser& parser) override
	{
		return m_value;
	}

	void unlink(const char* szName) override {}
};

// --------------------------------------------------------------

class WFunctionTokenizer
{
public:
	enum token_type
	{
		name,
		number,
		symbol,
	};

	const char* m_pEquation;
	const char* m_pNext;
	int m_len;
	token_type m_type;

	WFunctionTokenizer(const char* szEquation)
	: m_pEquation(szEquation), m_pNext(nullptr), m_len(0), m_type(name)
	{
	}

	static bool IsNameChar(char c)
	{
		if(c >= 'a' && c <= 'z')
			return true;
		if(c == '_')
			return true;
		if(c >= 'A' && c <= 'Z')
			return true;
		return false;
	}

	static bool IsDigit(char c)
	{
		if(c >= '0' && c <= '9')
			return true;
		return false;
	}

	void MeasureTokenLength()
	{
		if(IsNameChar(*m_pNext))
		{
			for(m_len = 1; IsNameChar(m_pNext[m_len]) || IsDigit(m_pNext[m_len]); m_len++)
			{
			}
			m_type = name;
		}
		else if(IsDigit(*m_pNext) || *m_pNext == '.')
		{
			int exps = 0;
			int decimals = 0;
			if(*m_pNext == '.')
				decimals++;
			for(m_len = 1; m_pNext[m_len] != '\0'; m_len++)
			{
				if(IsDigit(m_pNext[m_len]))
					continue;
				if(m_pNext[m_len] == '.' && decimals == 0 && exps == 0)
				{
					decimals++;
					continue;
				}
				if(m_pNext[m_len] == 'e' && IsDigit(m_pNext[m_len + 1]) && exps == 0)
				{
					exps++;
					continue;
				}
				break;
			}
			m_type = number;
		}
		else
		{
			m_len = 1;
			m_type = symbol;
		}
	}

	bool Next()
	{
		if(m_pNext)
			m_pNext += m_len;
		else
			m_pNext = m_pEquation;
		while(*m_pNext <= ' ' && *m_pNext != '\0')
			m_pNext++;
		if(*m_pNext == '\0')
		{
			m_pNext = nullptr;
			return false;
		}
		MeasureTokenLength();
		return true;
	}
};
}

// --------------------------------------------------------------

WFunctionParser::WFunctionParser()
{
	// Operators
	m_pNegate = new WFunctionBuiltIn(WFunctionBuiltIn::negate);
	m_pPlus = new WFunctionBuiltIn(WFunctionBuiltIn::plus);
	m_pMinus = new WFunctionBuiltIn(WFunctionBuiltIn::minus);
	m_pTimes = new WFunctionBuiltIn(WFunctionBuiltIn::times);
	m_pDivide = new WFunctionBuiltIn(WFunctionBuiltIn::divide);
	m_pModulus = new WFunctionBuiltIn(WFunctionBuiltIn::modulus);
	m_pExponent = new WFunctionBuiltIn(WFunctionBuiltIn::exponent);

	// Built-in constants (just a function with no parameters)
	addFunction("e", new WFunctionConstant(M_E), 0);
	addFunction("pi", new WFunctionConstant(M_PI), 0);

	// Built-in functions
	addFunction("abs", new WFunctionBuiltIn(WFunctionBuiltIn::_abs), 1);
	addFunction("acos", new WFunctionBuiltIn(WFunctionBuiltIn::_acos), 1);
#ifndef WINDOWS
	addFunction("acosh", new WFunctionBuiltIn(WFunctionBuiltIn::_acosh), 1);
#endif
	addFunction("asin", new WFunctionBuiltIn(WFunctionBuiltIn::_asin), 1);
#ifndef WINDOWS
	addFunction("asinh", new WFunctionBuiltIn(WFunctionBuiltIn::_asinh), 1);
#endif
	addFunction("atan", new WFunctionBuiltIn(WFunctionBuiltIn::_atan), 1);
#ifndef WINDOWS
	addFunction("atanh", new WFunctionBuiltIn(WFunctionBuiltIn::_atanh), 1);
#endif
	addFunction("ceil", new WFunctionBuiltIn(WFunctionBuiltIn::_ceil), 1);
	addFunction("cos", new WFunctionBuiltIn(WFunctionBuiltIn::_cos), 1);
	addFunction("cosh", new WFunctionBuiltIn(WFunctionBuiltIn::_cosh), 1);
#ifndef WINDOWS
	addFunction("erf", new WFunctionBuiltIn(WFunctionBuiltIn::_erf), 1);
#endif
	addFunction("floor", new WFunctionBuiltIn(WFunctionBuiltIn::_floor), 1);
	addFunction("ifzero", new WFunctionBuiltIn(WFunctionBuiltIn::_ifzero), 3);
	addFunction("ifnegative", new WFunctionBuiltIn(WFunctionBuiltIn::_ifnegative), 3);
	addFunction("gamma", new WFunctionBuiltIn(WFunctionBuiltIn::_gamma), 1);
	addFunction("lgamma", new WFunctionBuiltIn(WFunctionBuiltIn::_lgamma), 1);
	addFunction("log", new WFunctionBuiltIn(WFunctionBuiltIn::_log), 1);
	addFunction("softexp", new WFunctionBuiltIn(WFunctionBuiltIn::_softexp), 2);
	addFunction("max", new WFunctionBuiltIn(WFunctionBuiltIn::_max), -2 /*at least 1 param*/);
	addFunction("min", new WFunctionBuiltIn(WFunctionBuiltIn::_min), -2 /*at least 1 param*/);
	addFunction("normal", new WFunctionBuiltIn(WFunctionBuiltIn::_normal), 1);
	addFunction("sign", new WFunctionBuiltIn(WFunctionBuiltIn::_sign), 1);
	addFunction("sin", new WFunctionBuiltIn(WFunctionBuiltIn::_sin), 1);
	addFunction("sinh", new WFunctionBuiltIn(WFunctionBuiltIn::_sinh), 1);
	addFunction("sqrt", new WFunctionBuiltIn(WFunctionBuiltIn::_sqrt), 1);
	addFunction("tan", new WFunctionBuiltIn(WFunctionBuiltIn::_tan), 1);
	addFunction("tanh", new WFunctionBuiltIn(WFunctionBuiltIn::_tanh), 1);
}

WFunctionParser::~WFunctionParser()
{
	for(map<string, WFunction*>::iterator it = m_functions.begin(); it != m_functions.end(); it++)
		delete(it->second);
	delete(m_pNegate);
	delete(m_pPlus);
	delete(m_pMinus);
	delete(m_pTimes);
	delete(m_pDivide);
	delete(m_pModulus);
	delete(m_pExponent);
}

void WFunctionParser::add(const char* szEquations)
{
	vector<string> tokens;
	WFunctionTokenizer tokenizer(szEquations);
	while(tokenizer.Next())
	{
		size_t len = tokens.size();
		tokens.resize(len + 1);
		tokens[len] = string(tokenizer.m_pNext, tokenizer.m_len);
	}
	parseFunctionList(tokens);
}

void WFunctionParser::addFunction(const char* name, WFunctionNode* pRoot, int expectedParams)
{
	WFunction* pFunc = m_functions[name];
	if(pFunc)
	{
		onOverride(name);
		for(map<string, WFunction*>::iterator it = m_functions.begin(); it != m_functions.end(); it++)
			it->second->m_pRoot->unlink(name);
		delete(pFunc);
	}
	m_functions[name] = new WFunction(pRoot, expectedParams);
}

WFunction* WFunctionParser::getFunctionNoThrow(const char* name)
{
	map<string, WFunction*>::iterator it = m_functions.find(name);
	if(it == m_functions.end())
		return nullptr;
	return it->second;
}

WFunction* WFunctionParser::getFunction(const char* name)
{
	WFunction* pFunc = getFunctionNoThrow(name);
	if(!pFunc)
		throw Ex("No identifier named \"", name, "\" is currently defined");
	return pFunc;
}

int WFunctionParser::findOperatorWithLowestPrecidence(vector<string>& tokens, int start, int count)
{
	int priority = 100;
	int token = -1;
	int nests = 0;
	for(int i = 0; i < count; i++)
	{
		string& tok = tokens[start + i];
		if(tok.compare("(") == 0)
			nests++;
		else if(tok.compare(")") == 0)
		{
			nests--;
			if(i < 0)
				throw Ex("Unbalanced parentheses");
		}
		else if(nests == 0)
		{
			// negator		16
			// exponent		12
			// times/divide/modulus	8
			// plus/minus		4
			char c = tok[0];
			bool isOperand = WFunctionTokenizer::IsNameChar(c) || WFunctionTokenizer::IsDigit(c) || c == '.';
			if(!isOperand) // names and numbers aren't operators
			{
				if(c == '-' && (i == 0 || (!WFunctionTokenizer::IsDigit(tokens[start + i - 1][0]) && !WFunctionTokenizer::IsNameChar(tokens[start + i - 1][0]) && tokens[start + i - 1][0] != ')' && tokens[start + i - 1][0] != '.')))
				{
					if(priority >= 16)
					{
						priority = 16;
						token = start + i;
					}
				}
				else if(c == '^')
				{
					if(priority >= 12)
					{
						priority = 12;
						token = start + i;
					}
				}
				else if(c == '*' || c == '/' || c == '%')
				{
					if(priority >= 8)
					{
						priority = 8;
						token = start + i;
					}
				}
				else if(c == '+' || c == '-')
				{
					if(priority >= 4)
					{
						priority = 4;
						token = start + i;
					}
				}
				else
					throw Ex("Unrecognized operator: ", to_str(c));
			}
		}
	}
	return token;
}

WFunctionNode* WFunctionParser::parseMathOperator(std::vector<std::string>& variables, vector<string>& tokens, int start, int count, int index, int depth)
{
	// We've got a math operator (^,*,/,+,-,%)
	WFunctionNode* pLeft = nullptr;
	if(index > start)
		pLeft = parseFunctionBody(variables, tokens, start, index - start, depth);
	std::unique_ptr<WFunctionNode> hLeft(pLeft);
	WFunctionNode* pRight = nullptr;
	if(index < start + count - 1)
		pRight = parseFunctionBody(variables, tokens, index + 1, start + count - index - 1, depth);
	std::unique_ptr<WFunctionNode> hRight(pRight);
	if(!pLeft)
	{
		if(pRight && tokens[index].compare("-") == 0)
		{
			WFunctionCall* pFunc = new WFunctionCall(m_pNegate);
			pFunc->AddChild(hRight.release());
			return pFunc;
		}
		else
			throw Ex("Expected something before the operator: ", tokens[index].c_str());
	}
	if(!pRight)
		throw Ex("Expected something after the operator: ", tokens[index].c_str());
	char c = tokens[index][0];
	WFunctionCall* pFunc = nullptr;
	if(c == '^')
		pFunc = new WFunctionCall(m_pExponent);
	else if(c == '*')
		pFunc = new WFunctionCall(m_pTimes);
	else if(c == '/')
		pFunc = new WFunctionCall(m_pDivide);
	else if(c == '%')
		pFunc = new WFunctionCall(m_pModulus);
	else if(c == '+')
		pFunc = new WFunctionCall(m_pPlus);
	else if(c == '-')
		pFunc = new WFunctionCall(m_pMinus);
	else
		throw Ex("Unrecognized operator: ", to_str(c));
	pFunc->AddChild(hLeft.release());
	pFunc->AddChild(hRight.release());
	return pFunc;
}


void WFunctionParser::parseCommaSeparatedChildren(std::vector<std::string>& variables, WFunctionCall* pFunc, vector<string>& tokens, int start, int count, int depth)
{
	int childBegin = start;
	int nests = 0;
	for(int i = 0; i < count; i++)
	{
		string& tok = tokens[start + i];
		if(tok.compare("(") == 0)
			nests++;
		else if(tok.compare(")") == 0)
		{
			nests--;
			if(i < 0)
				throw Ex("Unbalanced parentheses");
		}
		else if(nests == 0)
		{
			if(tok.compare(",") == 0)
			{
				pFunc->AddChild(parseFunctionBody(variables, tokens, childBegin, start + i - childBegin, depth));
				childBegin = start + i + 1;
			}
		}
	}
	pFunc->AddChild(parseFunctionBody(variables, tokens, childBegin, start + count - childBegin, depth));
}

std::string WFunctionParser_joinTokens(std::vector<std::string>& tokens, int start)
{
	string s;
	for(int i = 0; i < start; i++)
		s += tokens[i];
	return s;
}

WFunctionNode* WFunctionParser::parseFunctionBody(std::vector<std::string>& variables, vector<string>& tokens, int start, int count, int depth)
{
	// Protect against maliciously designed formulas
	if(depth > 10000)
		throw Ex("Pathologically deep nesting");
	if(count <= 0)
		throw Ex("Empty expression following ", WFunctionParser_joinTokens(tokens, start));

	// Handle enclosing parens
	if(tokens[start].compare("(") == 0 && tokens[start + count - 1].compare(")") == 0)
	{
		int dep = 1;
		int i;
		for(i = 1; i < count - 1; i++)
		{
			if(tokens[start + i].compare("(") == 0)
				dep++;
			else if(tokens[start + i].compare(")") == 0)
			{
				dep--;
				if(dep == 0)
					break;
			}
		}
		if(i >= count - 1)
			return parseFunctionBody(variables, tokens, start + 1, count - 2, depth + 1);
	}

	// Generate a node
	if(count == 1)
	{
		char c = tokens[start][0];
		if(WFunctionTokenizer::IsNameChar(c))
		{
			// See if it's a variable
			for(size_t i = 0; i < variables.size(); i++)
			{
				if(variables[i].compare(tokens[start]) == 0)
				{
					// We've got a variable
					return new WFunctionVariable((int)i);
				}
			}

			// It must be a constant (parameterless function), so make a stub
			return new WFunctionCall(tokens[start].c_str());
		}
		else
		{
			// We've got a numeric constant
			double d;
			if(!parseNumber(tokens[start].c_str(), &d)) // SAKBox: was atof (1e999 gave infinity)
				throw Ex("Cannot parse symbol: ", tokens[start].c_str());
			return new WFunctionConstant(d);
		}
	}
	else
	{
		int index = findOperatorWithLowestPrecidence(tokens, start, count);
		if(index >= 0)
			return parseMathOperator(variables, tokens, start, count, index, depth + 1);
		else
		{
			// We've got a named function (like log(x), sin(y), or max(x,y))
			if(count < 3 || tokens[start + 1].compare("(") != 0 || tokens[start + count - 1].compare(")") != 0)
			{
				string s;
				for(int i = 0; i < count; i++)
					s += tokens[start + i];
				throw Ex("Cannot parse this portion of the expression: ", s.c_str());
			}
			WFunctionCall* pFunc = new WFunctionCall(tokens[start].c_str());
			std::unique_ptr<WFunctionCall> hFunc(pFunc);
			parseCommaSeparatedChildren(variables, pFunc, tokens, start + 2, count - 3, depth + 1);
			return hFunc.release();
		}
	}
}

void WFunctionParser::parseVariableNames(vector<string>& variables, vector<string>& tokens, int start, int count)
{
	for(int i = 0; i < count; i += 2)
	{
		char c = tokens[start + i][0];
		if(!WFunctionTokenizer::IsNameChar(c))
			throw Ex("Expected a variable name to start with a letter or '_'");
		variables.push_back(tokens[start + i]);
		if(i + 1 < count && tokens[start + i + 1].compare(",") != 0)
			throw Ex("Expected a comma between variable declarations");
	}
}

WFunctionNode* WFunctionParser::parseFunction(vector<string>& tokens, int start, int count)
{
	if(count == 0)
		return nullptr;

	// Find the '='
	int equalPos;
	for(equalPos = 0; equalPos < count; equalPos++)
	{
		if(tokens[start + equalPos].compare("=") == 0)
			break;
	}
	if(equalPos >= count)
		throw Ex("All functions must contain an '='");
	if(equalPos == 0)
		throw Ex("All functions must have a name");

	// Parse the variable names
	const char* szFunctionName = tokens[start].c_str();
	vector<string> variables;
	if(equalPos > 1)
	{
		if(tokens[start + 1].compare("(") != 0)
			throw Ex("Expected a '(' after ", szFunctionName);
		if(tokens[start + equalPos - 1].compare(")") != 0)
			throw Ex("Expected a ')' before the '='");
		parseVariableNames(variables, tokens, start + 2, equalPos - 3);
	}

	// Parse the body
	WFunctionNode* pRoot = parseFunctionBody(variables, tokens, start + equalPos + 1, count - (equalPos + 1), 0);
	addFunction(szFunctionName, pRoot, (int)variables.size());
	return pRoot;
}

void WFunctionParser::parseFunctionList(vector<string>& tokens)
{
	// Parse all the functions
	vector<WFunctionNode*> functions;
	int start = 0;
	for(int i = 0; i < (int)tokens.size(); i++)
	{
		if(tokens[i].compare(";") == 0)
		{
			functions.push_back(parseFunction(tokens, start, i - start));
			start = i + 1;
		}
	}
	functions.push_back(parseFunction(tokens, start, (int)tokens.size() - start));
}


// static
void WFunctionParser::test()
{
	{
		WFunctionParser mfp;
		mfp.add("f(x)=1/(1+e^-x)");
		WFunction* pFunc = mfp.getFunction("f");
		if(pFunc->m_expectedParams != 1)
			throw Ex("Wrong number of expected parameters");
		double x = 1.23456789;
		vector<double> params;
		params.push_back(x);
		double y = pFunc->call(params, mfp);
		if(std::abs(y - WMath::logistic(x)) > 1e-12)
			throw Ex("Wrong answer");
	}

	{
		WFunctionParser mfp;
		mfp.add("h(bob)=bob^2;somefunc(x)=3+blah(x,5)*h(x)-(x/foo);blah(a,b)=a*b-b;foo=3.2");
		WFunction* pFunc = mfp.getFunction("somefunc");
		if(pFunc->m_expectedParams != 1)
			throw Ex("Wrong number of expected parameters");
		double x = 1.1;
		vector<double> params;
		params.push_back(x);
		double y = pFunc->call(params, mfp);
		if(std::abs(y - 3.26125) > 1e-12)
			throw Ex("Wrong answer");
	}
}

