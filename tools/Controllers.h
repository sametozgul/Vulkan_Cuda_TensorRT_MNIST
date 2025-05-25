#ifndef CONTROLLERS_H
#define CONTROLLERS_H

#include <string>
#include <fstream>
#include <iostream>
namespace Controller{
    static bool isFileExists(const std::string& fileName){
        std::ifstream file(fileName.c_str());
        if(file.is_open()){
            file.close();
            return true;
        }else{
            return false;
        }
    }
    static void exitFatal(const std::string& message, int32_t messageCode){
        std::cerr << message << "\n";
        exit(messageCode);
    }
}


#endif // CONTROLLERS_H
