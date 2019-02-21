
def fact(n)
	if n == 1
		1
	else
		fact(n - 1) * n
	end
end

def txt(n)
	if n <= 3
		"N <= 3"
	else
		"N > 3"
	end
end

puts fact(5)
puts txt(3)

