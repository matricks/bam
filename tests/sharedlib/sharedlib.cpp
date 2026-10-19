#include <iostream> 
 
using namespace std; 

#if defined(_WIN32)
#define EXPORT_API __declspec(dllexport)
#else
#define EXPORT_API
#endif
EXPORT_API int shared_func()
{ 
        cout << "Hello world!" << endl;
        return 0; 
} 
