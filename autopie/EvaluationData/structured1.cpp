#include <iostream>
#include <cassert>

/*
 * This structured program introduces flow control based
 * on its arguments.
 */

int do_nothing(int x)
{
	int k = x - 1;
	return k + 1;
}

void handle_small_args()
{
	if (do_nothing(5) == 5)
	{
		return;
	}
	else 
	{
		std::cout << do_nothing(6) << "\n";
	}
}

bool handle_large_args()
{
	int k = do_nothing(4);

	return k < 4;
}

int main(int argc, char** argv)
{
	if (argc < 3)
	{
		handle_small_args();
		int n = do_nothing(42);
		int sum = 0;

		for (int i = 0; i < n; i++)
		{
			sum += do_nothing(i);
		}

		std::cout << sum << "\n";
	}
	else
	{
		if (!handle_large_args())
		{
			assert (false);
		}
	}

	std::cout << "End.\n";
}
