$X = 50
$Y = 40

$map = 0
$data = 0

def init
	map = Array((X+1)*(Y+1))
	data = Array((X+1)*(Y+1))
	for y = 0, y <= Y, y++
		for x = 0, x <= X, x++
			map[y * Y + x] = data[y * Y + x] = 0
		end
	end
	map[20 + Y * 6] = 1
	map[21 + Y * 6] = 1
end

def show 
	for y = 1, y < Y, y++
		for x = 1, x < X, x++
			if map[y * Y + x] == 0
				printf "_"
			else
				printf "#"
			end
		end
		puts ""
	end
end

def gen
	for y = 1, y < Y, y++
		for x = 1, x < X, x++
			data[y * Y + x] = map[y * Y + x]
		end
	end
	
	for y = 1, y < Y, y++
		for x = 1, x < X, x++
			if data[y * Y + x] == 0 
				b = data[y * Y + (x-1)] + data[y * Y + (x+1)] + data[(y-1) * Y + x] + data[(y+1) * Y + x]
				b = b + data[(y-1) * Y + (x-1)] + data[(y+1) * Y + (x-1)] + data[(y-1) * Y + (x+1)] + data[(y+1) * Y + (x+1)]
				if b == 3; map[y * Y + x] = 1; end
			elsif map[y * Y + x] == 1
				d = data[y * Y + (x-1)] + data[y * Y + (x+1)] + data[(y-1) * Y + x] + data[(y+1) * Y + x]
				d = d + data[(y-1) * Y + (x-1)] + data[(y+1) * Y + (x-1)] + data[(y-1) * Y + (x+1)] + data[(y+1) * Y + (x+1)]
				if d < 2; map[y * Y + x] = 0; end
				if d > 3; map[y * Y + x] = 0; end
			end
		end
	end
end

init()

for i = 0, i < 20, i++
	gen()
	show()
end
