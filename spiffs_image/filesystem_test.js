/*
 * Copyright: Collin Mulliner
 */

function OnStart() {
    bla = {
        test_name: "write read",
        number: 32
    };

    wr(JSON.stringify(bla));
    var x = rd();
    print("data: ");
    print(x);
    print("\n");

    var dir = FileSystem.listDir();
    print(JSON.stringify(dir));
    print("\n");
    
    var st = FileSystem.stat("fs_test.json");
    print("size: "+ JSON.stringify(st));
    print("\n");

    seektest();

    FileSystem.unlink("fs_test.json");

    dir = FileSystem.listDir();
    print(JSON.stringify(dir));
    print("\n");
}

function seektest() {
    var fp = FileSystem.open("fs_test.json", "r");
    var fplen = FileSystem.seek(fp, 0, 2);
    print("size: " + fplen + "\n");
    var data = new Uint8Array(1);
    FileSystem.seek(fp, fplen-1, 0);
    FileSystem.read(fp, data, 0, 1);
    if (String.fromCharCode(data[0]) == '}') {
        print("seek test success\n");
    }
    FileSystem.close(fp);
}

function rd() {
    var fp = FileSystem.open("fs_test.json", "r");
    var data = new Uint8Array(1024);
    FileSystem.read(fp, data, 0, 1024);
    FileSystem.close(fp);
    var dec = new TextDecoder();
    return dec.decode(data);
}

function wr(data) {
    var fp = FileSystem.open("fs_test.json", "w");
    FileSystem.write(fp, data);
    FileSystem.close(fp);
}

function OnEvent(evt) {
    print("event\n");
}
