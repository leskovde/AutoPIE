#include<iostream>

void write(int x)
{
	std::cout << x << "\n";
}

int read()
{
	int x;
	std::cin >> x;
	
	return x;
}

int main(void)
{
	int x = 1;
	int a = read();
	
	for (int i = 0; i < 0xffff; i++)
	{
		write(i);
	}
	
	if ((a % 2) == 0)
	{
		if (a != 0)
		{
			x *= -1;
		}
		else
		{
			x = 0;
		}
	}
	else
	{
		x++;
	}

	write(x);

	return 0;
}