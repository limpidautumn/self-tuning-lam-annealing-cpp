#!/usr/bin/env python3
from decimal import Decimal as D, getcontext

getcontext().prec = 100


def LamRate(p: D) -> D:
    if p <= D("0.15"):
        x = D("0.44") + D("0.56") * D(560) ** (-p / D("0.15"))
    elif p <= D("0.65"):
        x = D("0.44")
    else:
        x = D("0.44") * D(440) ** ((D("0.65") - p) / D("0.35"))
    return x


def Zeta(p: D) -> D:
    return -1 / (D("0.001") / (D("1.001") - LamRate(p))).ln()


if __name__ == "__main__":
    lp = [0.001, 0.002, 0.01, 0.02]
    for p in lp:
        fl = float(LamRate(D(p)))
        hl = fl.hex()
        fz = float(Zeta(D(p)))
        hz = fz.hex()
        print("p=", p, ", lambda=", hl, " (", fl, "), zeta=", hz, " (", fz, ")", sep="")
