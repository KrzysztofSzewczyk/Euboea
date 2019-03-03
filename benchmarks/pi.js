
String.prototype.sprintf = function() {
	var args = arguments;
	var index = 0;
	var x;
	var ins;
	var fn;

	return this.replace(String.prototype.sprintf.re, function() {
		if (arguments[0] == "%%") {
			return "%";
		}
		x = [];
		for (var i = 0; i < arguments.length; i++) {
			x[i] = arguments[i] || '';
		}
		x[3] = x[3].slice(-1) || ' ';
		ins = args[+x[1] ? x[1] - 1 : index++];
		return String.prototype.sprintf[x[6]](ins, x);
	});
};

function pi() {
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
    while(c > 0) {
        d = d % f
        e = d
        b = c - 1
        while(b > 0) {
            g = 2 * b - 1
            if(c != NM) {
                if(a[b] === undefined) {
                    a[b] = 0
                }
                d = d * b + f * a[b]
            } else {
                d = d * b + f * (f / 5)
            }
            a[b] = d % g
            d = d / g
            b = b - 1
        }
        console.log("%04d".sprintf(e + d / f))
        if(cnt % 16 == 0) {
            console.log("")
        }
        cnt = cnt + 1
        c = c - 14;
    }
}
 
console.log(pi())
