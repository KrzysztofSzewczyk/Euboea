
function pi()
    N = 14 * 5000
    NM = N - 14
    a = {}
    d = 0
    e = 0
    g = 0
    h = 0
    f = 10000
    cnt = 1
    c = NM
    while c > 0 do
        d = math.mod(d, f)
        e = d
        b = c - 1
        while b > 0 do
            g = 2 * b - 1
            if c ~= NM then
                if a[b] == nil then
                    a[b] = 0
                end
               
                d = d * b + f * a[b]
            else
                d = d * b + f * (f / 5)
            end
            a[b] = math.mod(d, g)
            d = d / g
            b = b - 1
        end
        io.write(string.format("%04d", e + d / f))
        if math.mod(cnt, 16) == 0 then
            print("")
        end
        cnt = cnt + 1
        c = c - 14;
    end
end
 
print(pi())
