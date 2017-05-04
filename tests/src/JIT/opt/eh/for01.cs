using System;
using System.Runtime.CompilerServices;

class For01
{
    public int[] array;
    public For01(int size)
    {
        array = new int[size];
    }
}

class Test 
{
    static int Main()
    {
        For01 f = new For01(99);
        int sum = 0;
        int i = 101;
        try
        {
            for (i = val(0); i < val(99); i++)
            {
                f.array[i] = val(i);
                sum = sum + i;
            }
        }
        catch (System.Exception e)
        {
            Console.WriteLine("i is {0}", Convert.ToString(i));
        }

        return sum;
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    static int val(int x)
    {
        return x;
    }
}
