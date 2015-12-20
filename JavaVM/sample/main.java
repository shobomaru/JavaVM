
class Main
{
	public static void main(String args[])
	{
		int a = 1;
		a = a + 2;
		a = a * 3;
		output(a);

		a -= 7;
		output(a);

		if (a > 2)
			a -= 1;
		else
			a += 1;
		output(a);

		for (int i = 0; i < 7; ++i)
			a++;
		output(a);

		int[] ary = {a};
		output(ary[0]);

		a = sub(a, 3);
		output(a);

		//a /= 0;
		try
		{
			a /= 0;
			output(1);
		}
		catch(java.lang.ArithmeticException e)
		{
			output(0);
		}
		finally
		{
			output(2);
		}

		int b = local_ / a;
		output(b);
	}

	static int sub(int minuend, int subtrahend)
	{
		return minuend - subtrahend;
	}

	static int local_ = 1234567890;

	public static native void output(int val);
}
