// Import the modules
const fs = require("fs");
const path = require("path");

// Get the binary file name from the command line argument
const binaryFileName = process.argv[2];

// Check if the file name is valid
if (!binaryFileName) {
  console.error("Please provide a binary file name");
  process.exit(1);
}

function reverseByte(byte) {
  // Initialize the result as 0
  let result = 0;
  // Loop through the 8 bits of the byte
  for (let i = 0; i < 8; i++) {
    // Shift the result to the left by one bit
    result = result << 1;
    // Add the least significant bit of the byte to the result
    result = result | (byte & 1);
    // Shift the byte to the right by one bit
    byte = byte >> 1;
  }
  // Return the result
  return result;
}

// Read the binary file as a buffer
fs.readFile(binaryFileName, (err, buffer) => {
  // Handle any errors
  if (err) {
    console.error(err.message);
    process.exit(2);
  }

  // Get the length of the buffer
  const length = buffer.length;

  // Initialize an array to store the reversed bytes
  const reversedBytes = [];

  // Loop through the buffer and reverse each byte
  for (let i = 0; i < length; i++) {
    const byte = buffer[i];
    const reversedByte = reverseByte(byte);
    reversedBytes.push(reversedByte);
  }

  // Convert the array to a string with hexadecimal format and comma separators
  const hexString = reversedBytes.map(b => "0x" + b.toString(16)).join(", ");

  // Create the C header file content with the array declaration
  const headerFileContent = `uint8_t fnt8x16[${length}] = {${hexString}};`;

  // Get the text file name by changing the extension to .h
  const textFileName = path.join(path.dirname(binaryFileName), path.basename(binaryFileName, path.extname(binaryFileName)) + ".h");

  // Write the text file with the header file content
  fs.writeFile(textFileName, headerFileContent, (err) => {
    // Handle any errors
    if (err) {
      console.error(err.message);
      process.exit(3);
    }

    // Log a success message
    console.log(`Text file ${textFileName} created successfully`);
  });
});