
#ifndef TPLCC_ERROR_H
#define TPLCC_ERROR_H

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <cstdint>

#include "scanner.h" 
#include "code-buffer.h"

class Error {
    std::string msg;
    std::string hintMsg;
    std::tuple<std::uint32_t, std::uint32_t> range;

public:
    Error() = default;
    Error(std::tuple<CodeBuffer::Offset, CodeBuffer::Offset> range,
          std::string msg, std::string hintMsg = "")
        : range(range), msg(std::move(msg)), hintMsg(std::move(hintMsg)) {}

    std::string hint() const {
        return hintMsg;
    }

    std::string errorMessage() const {
        return msg;
    }

    std::tuple<CodeBuffer::Offset, CodeBuffer::Offset> codeRange() const {
        return range;
    }
};

struct IReportError {
    virtual void reportsError(Error error) = 0;
    virtual ~IReportError() = default;
};

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
public:
    ErrorReporter(
        std::string filename,
        std::ostream& outStream
    ) : filename(filename) {}

    void reportsError(Error error);
    void outputErrorMessagesTo(std::ostream& os);
};

#endif