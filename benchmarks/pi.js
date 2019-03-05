
function pi() {
    var N = 14 * 5000;
    var NM = N - 14;
    var a = {};
    var d = 0;
    var e = 0;
    var g = 0;
    var h = 0;
    var f = 10000;
    var cnt = 1;
    var c = NM;
    while(c > 0) {
        d = d % f;
        e = d;
        var b = c - 1;
        while(b > 0) {
            g = 2 * b - 1;
            if(c != NM) {
                if(!a[b]) {
                    a[b] = 0;
                }
                d = d * b + f * a[b];
            } else {
                d = d * b + f * (f / 5);
            }
            a[b] = d % g;
            d = d / g;
            b = b - 1;
        }
        process.stdout.write((Math.floor(e + d / f)).toString().padStart(4, "0"));
        if(cnt % 16 === 0) {
            console.log("");
        }
        cnt = cnt + 1;
        c = c - 14;
    }
}
 
console.log(pi());
