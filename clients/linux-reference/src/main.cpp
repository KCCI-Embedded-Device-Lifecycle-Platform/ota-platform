#include "reference_client_app.hpp"
#include <iostream>
using namespace std;

int main()
{
    ReferenceClientApp app;

    if(!app.initialize())
    {
        cerr << "Failed to initialize Linux reference client\n";
        return 1;
    }

    return app.run();
}
