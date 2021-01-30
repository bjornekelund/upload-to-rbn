# upload-to-rbn
A small utility for uploading FT8 decodes from a 
Red Pitaya [125-14](https://github.com/pavel-demin/red-pitaya-notes) or
[122.88-16](https://github.com/pavel-demin/red-pitaya-notes) based, multi-band FT8 receiver to the [Reverse Beacon Network](http://www.reversebeacon.net) via [RBN Aggregator](http://www.reversebeacon.net/pages/Aggregator+34). Call sign and grid are both set by RBN Aggregator.
Derivative work from [Pavel Demin](https://github.com/pavel-demin)'s `upload-to-pskreporter.c`.

Usage: 

`./upload-to-rbn broadcast-IP-address broadcast-UDP-port filename`
