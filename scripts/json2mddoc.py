#!/usr/bin/python2

import json
import sys

def getFunctions(fname):
    fp = open(fname, "r")
    if fp == None:
        return None
    line = " "
    isdoc = False
    example = False
    outdoc = []
    cd = ""
    while line != None and len(line) > 0:
        line = fp.readline()
        if "/* jsondoc" in line:
            isdoc = True
            continue
        if "*/" in line and isdoc:
            isdoc = False
            outdoc.append(cd)
            cd = ""
            continue
        if isdoc:
            cd += line.strip('\n')
            if "\"example\":" in line or "\"longtext\":" in line:
                example = True
                continue
            if example:
                if line[0] == '"':
                    example = False
                else:
                    cd += "\\n"
    #print outdoc
    return outdoc

def sortfuncs(fcns):
    sfcn = []
    fn = {}
    out = []
    for f in fcns:
        fcn = None
        try:
            fcn = json.loads(f)
        except:
            sys.stderr.write(f+"\n")
            sys.exit(1)
        if "class" in fcn:
            pass
        else:
            fn[fcn["name"]] = fcn
            sfcn.append(fcn["name"])
    sfcn.sort()
    for f in fcns:
        if "class" in f:
            out.append(json.loads(f))
            break
    for f in sfcn:
        out.append(fn[f])
    return out

def printfuncs(fncs):
    idx = []
    for fcn in fncs:
        if "class" in fcn:
            continue
        fn = fcn['name']
        for a in fcn['args']:
            fn += a['name']
        idx.append((fcn["name"],fn))
    for fcn in fncs:
        if "class" in fcn:
            print("# " + fcn['class'] + "\n")
            if "longtext" in fcn:
                print(fcn['longtext'])
            else:
                print(fcn['text'])
            if len(fncs) > 1:
                print("## Methods\n")
                for fn,f in idx:
                    print("- [" + fn + "](#" + f.lower() + ")")
                print("\n---\n")
            continue
        fn = fcn['name']
        fn += "("
        for a in fcn['args']:
            fn += a['name'] + ","
        if fn[len(fn)-1] == ",":
            fn = fn[0:len(fn)-1]
        fn += ")"
        print "## " + fn + "\n"
        if "longtext" in fcn:
            print(fcn['longtext'] + "\n")
        else:
            print(fcn['text'] + "\n")
        for a in fcn['args']:
            print("- " + a["name"] + "\n")
            print("  type: " + a["vtype"] + "\n")
            print("  " + a['text'] + "\n")
        if "return" in fcn:
            print("**Returns:** " + fcn['return'] + "\n")
        print("```\n"+fcn["example"]+"\n```\n")


x = getFunctions(sys.argv[1])
#for p in x:
#    print(p)
#printfuncs(x)

sx = sortfuncs(x)
printfuncs(sx)
