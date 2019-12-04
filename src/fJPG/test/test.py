def hex(x):
    return format(x, "02X")

f = open("t2.jpeg", 'rb')
num = f.read()
h = map(hex, num)
ls = list(zip(num,h))
m = 0
eLen = 0
Ss = -1
Sh = -1
Ah = -1
Al = -1
for c in ls:
    if m == 0:
        if c[1] == 'FF':
            m = 1
    elif m == 1:
        m = 2 if c[1] == 'DA' else 0
    elif m == 2:
        eLen = c[0]
        m = 3
    elif m == 3:
        eLen = (eLen << 8) + c[0] - 5
        m = 4
    elif m == 4:
        eLen -= 1
        if eLen == 0:
            m = 5
            eLen = 3
        print(c[1], end = ' ' if eLen % 2 else '\n')
    elif m == 5:
        eLen -= 1
        if eLen < 0:
            print("\n%i\t%i\t%i\t%i\n" % (Ss,Sh, Ah, Al))
            m = 0
        else:
            if eLen == 2:
                Ss = c[0]
            elif eLen == 1:
                Sh = c[0]
            elif eLen == 0:
                Ah = (c[0] & 0xF0)>>4
                Al = (c[0] & 0x0F)
