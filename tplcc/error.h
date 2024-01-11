
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

# WHAT IS THE FORMAT OF ERROR MESSAGES #

 We can devide errors in a programme into two kinds:
1. The first kind are those happens in a macro expansion
2. The other kind are those happens outside a macro expansion

For an error that occurs outside a macro, We should print the line where the error happens,
and highlight the part that causes the error, For example:

foo.c: The number 0.3ef has no exponent part.

10 |     int a = 0.3ef;
                 ^^^^^

If the error occurs in a #define expansion, we should print and hightlight the macro in the source code first,
shows the definition of the macro, demonstrate the process of the expansion, and finally show the error in the expaned code.
for example:

```

foo.c: Undefined varaible "abc"

10 |            print("%d\n", abc);

It occurs in this macro:

10 | PRINT_LIST(abc, message);
     ^^^^^^^^^^^^^^^^^^^^^^^^

The macro has following defintion:

3  | #define PRINT_LIST(list, message) \
4  | for (int i = 0; i < list.size; i++) { \
   |     PRINT_WITH_MESSAGE(list, i, message); \
   | } \

and is expanded into following text:

10 | for (int i = 0; i < abc.size; i++) {
                         ^^^ (undefined variable)

   |     if (isEmpty(abc[i])) {
                     ^^^ (undefined variable)

   |          printf("item %d is empty.\n", i);
   |     }
   | }
```

If the error is inside some #include directive, we should print the included file's name first, then print out the error.

```
In the included file "abc.h"

foo.c: Undefined varaible "abc"

10 | abc = 3;

```

---

*/

// Collect errors from transformation units (lexer, parser i.e.), add infomation 
// about the location where the error occurs, shows the macro expansion if the
// error is under a macro expansion, and finally output these error message to
// an ostream.
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