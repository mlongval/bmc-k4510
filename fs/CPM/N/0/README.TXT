N: -- THE NETWORK DRIVE  (RESERVED; NOT BUILT YET)
--------------------------------------------------
This drive letter belongs to the network. Nothing else may
claim it: no library, no scratch area, no second copy of A:.
It is empty because the plumbing is not written yet.

WHY ONE DRIVE AND NOT TWO
On this machine the SCHEME lives in the NAME, not in the
device. That is the Meatloaf rule -- a URL is a file name --
and CD tnfs://host/dir extends it to directories. FujiNet and
Meatloaf are not two kinds of place; they are two URL schemes
over one namespace, so splitting them across two drive letters
would divide something the machine deliberately joined.
N: is the letter that has meant "network" on every machine
FujiNet ever touched.

WHAT IT WILL BE
CP/M names are 8.3 and cannot hold a URL, so N: is a MOUNT
POINT rather than an address: N:0 shows the machine's current
remote directory, and you move it with the K/OS shell
(CD tnfs://host/dir, CD .., CD -) before coming in here.
DIR N: then lists what the server has, and a CP/M program
opens N:NAME.EXT like any other file.
