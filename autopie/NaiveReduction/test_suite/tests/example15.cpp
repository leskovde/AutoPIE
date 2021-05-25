#include <iostream>

/**
 * Error when working with public members.
 */

class Record
{
public:
    std::string str1 = nullptr;
    std::string str2 = nullptr;
    int year;

    Record()
    {
    }

    Record(std::string b, std::string m, int y) : str1(b), str2(m), year(y)
	{
	}
};

int
main()
{
    Record record1("str1", "str2", 42);

    Record record2;
    record2.str1 = "str1_1";
    record2.year = 51;

    std::cout << "Record 1\n";
    std::cout << record1.str1 << "\n";
    std::cout << record1.str2 << "\n"; 
    std::cout << record1.year << "\n";

    std::cout << "Record 2\n";
    std::cout << record2.str1 << "\n"; 
    std::cout << record2.str2 << "\n"; 
    std::cout << record2.year << "\n";
    
    return (0);
}