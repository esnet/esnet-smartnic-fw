#ifndef INCLUDE_STRINGIZE_H
#define INCLUDE_STRINGIZE_H

#define STRINGIZE0(_x) #_x
#define STRINGIZE1(_x) STRINGIZE0(_x)
#define STRINGIZE2(_x) STRINGIZE1(_x)

#endif // INCLUDE_STRINGIZE_H