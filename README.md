# pnganography

PNG image steganography utility

## Usage

```bash
Usage: pnganography encode|decode <key.png> <secret> <output>
```

For encoding, the 4th argument (&lt;secret&gt;) should be the data that you wish to embed

For decoding, the argument should be the image with the embedded data

## Info

This program functions by splitting the input file into bits and placing them, one by one, in the least-significant bit of every RGB color value in a PNG. The following functions can help you determine how large your input and PNGs should be:

```
minimum_size_of_image_in_pixels = (input_bytes * 8) / 3
maximum_size_of_input_in_bytes = (image_pixels * 3) / 8
```

Note: This applies only for RGB PNGs. For grayscale, replace the 3s with 1s

## Compiling

```bash
make && sudo make install
```

## Testing

```bash
make test
```

Note: You make specify your own hashing function in the Makefile. It is used to compare the input and output.

## Pronunciation

Just so there is no GIF/JIF-like confusion, this program is pronounced:

```
ping-a-nography
```
