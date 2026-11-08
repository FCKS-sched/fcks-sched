#include <iostream>
#include <xgboost/c_api.h>

int main() {
    // XGBoost API call to check the library version
    XGBoosterCreate(nullptr, 0, nullptr); 
    std::cout << "XGBoost C++ environment is successfully linked and running!" << std::endl;
    return 0;
}