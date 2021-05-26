#include <iostream>

/**
 * Error when working with private members.
 */

class Record
{
    Record* self = nullptr;

    void Join(Record* ptr)
    {
	    self = ptr;
    }

    void Initialize()
    {
        self->Join(this);
    }
    
    void DebugPrint()
    {
        std::cout << "Constructing at " << self << "\n";
    }
	
public:
    std::string str1;
    std::string str2;
    int year;

    Record()
    {

    }

    Record(std::string b, std::string m, int y) : str1(b), str2(m), year(y)
    {
        DebugPrint();
    	
        Initialize();
    }
};

int
main()
{
    Record record1("str1", "str2", 42);

    Record record2;
    record2.str1 = "str1_1";
    record2.year = 51;

    std::cout << record1.str1 << " " << record1.str2 << " " << record1.year << "\n";
    std::cout << record2.str1 << " " << record2.str2 << " " << record2.year << "\n";

    return (0);
}
