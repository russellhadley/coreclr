using System;
using System.IO;
using System.Text;
using System.Runtime.CompilerServices;
using System.Collections.Generic;
using System.Threading.Tasks;

class Foreach01
{

    public Foreach01() {}

    [MethodImpl(MethodImplOptions.NoInlining)]
    public async Task<int> Run() 
    {
        int sum = Value(1);
        var list = new List<int>() { Value(2), Value(4), Value(8)};

        foreach (var e in list)
        {
            sum = sum + e;
        }

        sum = sum + Value(42);
        
        string path = "t.csv";
        using (FileStream fs = File.Open(path, FileMode.Open, FileAccess.Read, FileShare.None))
        {
            var result = new byte[fs.Length];
            
            await fs.ReadAsync(result, 0, (int)fs.Length);

            var str = Encoding.UTF8.GetString(result);
            
            var lines = str.Split(new string[] {Environment.NewLine}, StringSplitOptions.RemoveEmptyEntries);

            foreach(var line in lines)
            {
                var fields = line.Split(',');

                foreach (var field in fields)
                {
                    int number;
                    bool success = Int32.TryParse(field, out number);
                    if (success)
                    {
                        sum = sum + number;
                    }
                }
            }

        }

        Console.WriteLine("sum is {0}", Convert.ToString(sum));

        return sum;
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    public int Value(int x) { return x;}
}

class Test 
{
    static int Main()
    {
        Foreach01 f1 = new Foreach01();

        Task<int> test = Task.Run(() => f1.Run());
        test.Wait();
        int result = test.Result;
        
        return result;
    }
}
