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

#ifndef __GFUNCTION_H__
#define __GFUNCTION_H__

#include <string.h>
#include <vector>
#include <string>
#include <map>

namespace WClasses
{

class WFunctionNode;
class WFunctionCall;
class WFunctionBuiltIn;
class WFunctionStub;
class WFunctionParser;

struct strCmp
{
	bool operator()(const char* a, const char* b) const { return strcmp(a, b) < 0; }
};

/// This class represents a math function. (It might be used, for example, in a plotting tool.)
class WFunction
{
public:
	WFunctionNode* m_pRoot;
	int m_expectedParams; // an "int" because negative values are used to indicate variable numbers of parameters, such as in a "max" function.

	/// This constructor is used internally by WFunctionParser. Typically you
	/// will call WFunctionParser::GetFunction to obtain a pointer to one of these.
	WFunction(WFunctionNode* pRoot, int expectedParams);
	~WFunction();

	/// Calls the function and returns the results. (This does not check
	/// that the right number of parameters are passed in, so be sure that
	/// the number of parameters matches m_expectedParams before you call
	/// this method.)
	double call(std::vector<double>& params, WFunctionParser& parser);
};

/// This class parses math equations. (This is useful, for example, for plotting tools.)
class WFunctionParser
{
protected:
	std::map<std::string, WFunction*> m_functions;
	WFunctionBuiltIn* m_pNegate;
	WFunctionBuiltIn* m_pPlus;
	WFunctionBuiltIn* m_pMinus;
	WFunctionBuiltIn* m_pTimes;
	WFunctionBuiltIn* m_pDivide;
	WFunctionBuiltIn* m_pModulus;
	WFunctionBuiltIn* m_pExponent;

public:
	/// szEquations is a set of equations separated by semicolons. For example,
	/// it could parse "f(x)=3*x+2" or "f(x)=(g(x)+1)/g(x); g(x)=sqrt(x)+pi" or
	/// "h(bob)=bob^2;somefunc(x)=3+blah(x,5)*h(x)-(x/foo);blah(a,b)=a*b-b;foo=3.2"
	/// Built in constants include: e, and pi.
	/// Built in functions include: +, -, *, /, %, ^, abs, acos, acosh, asin, asinh,
	/// atan, atanh, ceil, cos, cosh, erf, floor, gamma, lgamma, log, max, min,
	/// sin, sinh, sqrt, tan, and tanh. These generally have the same meaning as in C,
	/// except '^' means exponent, "gamma" is the gamma function, and max and min
	/// can support any number of parameters >= 1. "ifzero" returns its second
	/// parameter if its first parameter is zero (when rounded), otherwise returns
	/// its third parameter. "ifnegative" returns its second parameter if its first
	/// parameter is negative, otherwise returns its third parameter.
	/// (Some of these functions may not
	/// not be available on Windows, but most of them are.)
	/// You can override any built in constants or functions with your own variables
	/// or functions, so you don't need to worry too much about name collisions.
	/// Variables must begin with an alphabet character or an underscore.
	/// Multiplication is never implicit, so you must use a '*' character to multiply.
	/// Whitespace is ignored. If it can't parse something, it will throw an exception.
	/// Linking is done lazily, so it won't complain about undefined identifiers
	/// until you try to call the function.
	WFunctionParser();
	virtual ~WFunctionParser();

	/// Performs unit tests for this class. Throws an exception if there is a failure.
	static void test();

	/// Parses the given string and adds its contents to the set of known functions and values.
	void add(const char* szEquations);

	/// This method is called whenever a function is added with the same name as an existing function,
	/// replacing its functionality. For example, if you want to disallow function overriding,
	/// you could override this method to throw an exception.
	virtual void onOverride(const char* name) {}

	/// Returns a pointer to the specified function. (Don't include any
	/// parentheses in the function name.) Throws if it is not found.
	WFunction* getFunction(const char* name);

	/// Returns a pointer to the specified function. (Don't include any
	/// parentheses in the function name.) Returns NULL if it is not found.
	WFunction* getFunctionNoThrow(const char* name);

protected:
	WFunctionNode* parseMathOperator(std::vector<std::string>& variables, std::vector<std::string>& tokens, int start, int count, int index, int depth);
	void parseCommaSeparatedChildren(std::vector<std::string>& variables, WFunctionCall* pFunc, std::vector<std::string>& tokens, int start, int count, int depth);
	WFunctionNode* parseFunctionBody(std::vector<std::string>& variables, std::vector<std::string>& tokens, int start, int count, int depth);
	WFunctionNode* parseFunction(std::vector<std::string>& tokens, int start, int count);
	void parseVariableNames(std::vector<std::string>& variables, std::vector<std::string>& tokens, int start, int count);
	void parseFunctionList(std::vector<std::string>& tokens);
	int findOperatorWithLowestPrecidence(std::vector<std::string>& tokens, int start, int count);
	void addFunction(const char* name, WFunctionNode* pRoot, int expectedParams);
};

} // namespace WClasses

#endif // __GFUNCTION_H__
