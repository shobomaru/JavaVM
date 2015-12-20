class Fibonacci
{
	private static final int  TIMES = 1*1000*1000*1000;
	public static void main(String[] args)
	{
		int total = 1;
		int prev = 1, pprev = 1;
		for (int i = 2; i < TIMES; i++)
		{
			pprev = prev;
			prev = total;
			total = prev + pprev;
		}
		output(total);
	}
	public static native void output(int val);
	//public static void output(int val){System.out.println(val);}
}
