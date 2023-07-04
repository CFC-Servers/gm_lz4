<html>
  <head>
    <script src=" https://cdn.jsdelivr.net/npm/lz4@0.6.5/build/lz4.min.js"></script>

    <script>
      var Buffer = require('buffer').Buffer
      var LZ4 = require('lz4')

      // Polyfill
      if (!ArrayBuffer.isView) {
        ArrayBuffer.isView = function(obj) {
          return obj !== null && obj !== undefined && obj.buffer instanceof ArrayBuffer;
        };
      }

      function compress(data) {
        var input = new Buffer(data)
        var output = new Buffer(LZ4.encodeBound(input.length))
        var compressedSize = LZ4.encodeBlock(input, output)
        return output.slice(0, compressedSize)
      }

      function decompress(compressed) {
        var input = new Buffer(compressed)
        var output = new Buffer(input.length * 255)
        var decompressedSize = LZ4.decodeBlock(input, output)
        return output.slice(0, decompressedSize)
      }

      function Compress(data) {
        gmod.Report(compress(data).toString());
      }

      function Decompress(compressed) {
        var decompressed = decompress(compressed)
        gmod.Report(decompressed.toString())
      }

    </script>
  </head>
</html>
