
#ifndef TPLCC_ERROR_REPORTING_H
#define TPLCC_ERROR_REPORTING_H

#include <memory>
#include <string>
#include <vector>
#include <optional>

#include "scanner.h"

class Error {
public:
    Error() = default;
    virtual ~Error() = default;
    virtual std::string hint() { return ""; }
    virtual std::string errorMessage() {  return ""; }
};

class StringError : public Error {
    std::string msg;
    std::string hintMsg;

public:
    StringError(std::string msg, std::string hintMsg = "") : msg(msg), hintMsg(hintMsg) {}

    std::string hint() {
        return hintMsg;
    }

    std::string errorMessage() {
        return msg;
    }
};

struct IReportError {
    virtual void reportsError(std::unique_ptr<Error> error, CodeRange pos) = 0;
    virtual ~IReportError() = default;
};

template<typename T, typename... Args>
void reportsError(IReportError& errOut, CodeRange pos, Args&&... args) {
    errOut.reportsError(std::make_unique<T>(std::forward<Args>(args)...), pos);
}

/* 
We can devide errors in a programme into two kinds:
1. The first kind are those happens in a macro expansion
2. The other kind are those happens outside a macro expansion

An error message includes following sections
1. The file where the error occurs.
1. A error message shows the reason of the error
2. A text shows the location where the error happens

For an error that is outside a macro, We should print the line where the error happens,
and highlight the part that causes the error.

For example, here is a lexical error:

In the file "foo.c": The number 0.3ef has no exponent part.

10 |     int a = 0.3ef;
                 ^^^^^

We information we should keep in order to print this error?
1. The error message
2. The location data

The error message usually contains the error element(s), and a method toString that generates
the error message, in which usually contains the error element(s).

A error location message is a struct defined as follow:

struct ErrorLocation {
    std::unique_ptr<Error> error; // reference to that error;
    size_t lineNumber;
    std::tuple<int, int> highlightRange;
    std::string suggestion;
}

--

If the error occurs in a macro expansion. We should print and hightline the macro in the source code first,
shows the definition of the macro, demonstrate the process of the expansion, and finally show the error in the expaned code.

here is an example.

```
In the file "foo.c"

Error occurs in this macro:

10 | PRINT_LIST(abc, message);
     ^^^^^^^^^^^^^^^^

     variable "abc" is undefined

The macro has following defintion:

3  | #define PRINT_LIST(list, message) \
4  | for (int i = 0; i < list.size; i++) { \
   |     PRINT_WITH_MESSAGE(list, i, message); \
   | } \

It's expanded into following text:

10 | for (int i = 0; i < abc.size; i++) {
                         ^^^ (undefined variable)

   |     if (isEmpty(abc[i])) {
                     ^^^ (undefined variable)

   |          printf("item %d is empty.\n", i);
   |     }
   | }
```

The error passed to the reportsError by any transformation unit (lexer, parser, syntax analyzer etc.) only contains
the most basic error message i.e. `variable "foo" is undefined`. This is not enough for users. We shall print out the
line where the error occurs, and highlight the part that causes the error.

Since the error itself doesn't contains any information on its occuring position, in addition to the error object itself,
a `CodePos` object is needed when we call the method `reportsError`. The later should be obtained by the IContext
interface, which will be injected into every transformation unit

If the error is not from a expanded macro, we can simply store the part we are going to hightlight in a ErrorLocation
object.

*/


// The interface exposes error strings, these structs are all private.

/*
* depends on the context, a `CodePos` struct can represent a position
* in a source code or a position in a macro-expanded text 
*/

// Objects of this class collect errors from transformation units (lexer, parser i.e.),
// add infomation about the location where the error occurs, shows the macro expansion
// if the error is under a macro expansion, and finally output these error message
// to a ostream.
class ErrorReporter: IReportError {
    std::string filename;
    std::vector<Error> listOfErrors;
    IGetText& objGetText;
public:
    ErrorReporter(
        std::string filename,
        std::ostream& outStream,
        IGetText& objGetText
    ) : filename(filename), objGetText(objGetText) {}

    void reportsError(std::unique_ptr<Error> error, CodePos pos);
    void outputErrorMessagesTo(std::ostream& os);
};

#endif