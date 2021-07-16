#include <stdio.h>
#include <iostream>

struct Data {
    int id;
    int data[2];
};

int main(){
    Data old_data;

    old_data.id = 2;
    old_data.data[0] = 2;
    old_data.data[1] = 4;

    //Sending Side
    char raw_data[sizeof(old_data)];
    memcpy(raw_data, &old_data, sizeof(old_data));

    //Receiving Side
    Data new_data; //Re-make the struct
    memcpy(&new_data, raw_data, sizeof(new_data));

    std::cout << new_data.id;
    std::cout << " : ";
    std::cout << new_data.data[1];
    std::cout << "\n";
    return 0;
}