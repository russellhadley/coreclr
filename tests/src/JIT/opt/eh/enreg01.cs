using System;
using System.Runtime.CompilerServices;

public class ValueException : System.Exception
{
    public int x;

    public ValueException(int val) { x = val; }
};

class Enreg01
{
    int val;
    double dist;

    public Enreg01(int x) {
        val = x;
        dist = (double)x;
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    public int foo(ref double d) { return (int)d; }

    [MethodImpl(MethodImplOptions.NoInlining)]
    public int Run()
    {
        int sum = val;

        try {
            TryValue(97);
        }
        catch (ValueException e)
        {
            Console.WriteLine("Catching {0}", Convert.ToString(e.x));
            sum += val + e.x;
            foo(ref dist);
            sum += val;
        }

        return sum;
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    public int TryValue(int y) 
    {
        if (y == 97) 
        {
            Console.WriteLine("Throwing 97");
            throw new ValueException(97);
        }
        else
        {
            return y;
        }
    }
}

class Test 
{
    static int Main()
    {
        Enreg01 e1 = new Enreg01(1);
        int result = e1.Run();
        Console.WriteLine("returning {0}", Convert.ToString(result));
        return result;
    }
}
