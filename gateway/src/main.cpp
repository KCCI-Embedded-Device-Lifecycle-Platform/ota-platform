#include "gateway_app.hpp"
#include <iostream>
using namespace std;

int main()
{
    GatewayApp app;

    if(!app.initialize())
    {
        cerr << "Failed to initialize gateway\n";
        return 1;
    }

    return app.run();
}
