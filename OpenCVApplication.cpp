/*
* Author:	Hunor Debreczeni
* Date:		Apr. 17th, 2021
*/
#include "stdafx.h"
#include "common.h"
#include <limits.h>
#include <iostream>
#include <fstream>


enum OPERATION {
	COMPRESS = 1,
	DECOMPRESS
};

typedef struct arguments {
	int operation = 0;
	char* input = nullptr;
	char* output = nullptr;
	boolean error = false;
	boolean multiLevel = false;
	char* errorCause = nullptr;
} Arguments, * pArguments;

void logArguments(Arguments args);

Arguments parseArguments(int argc, char** argv);

void writeValues(std::ofstream* out, uchar* color, uchar* occurrence);

void computePDF(Mat img, float pdf[256]);

Mat multiLevelThresholding(Mat src);

void compressImage(char* input, char* output, boolean multiLevel);

void decompressImage(char* input, char* output);

int main(int argc, char** argv)
{
	Arguments arguments = parseArguments(argc, argv);
	if (arguments.error) {
		std::cout << "ERROR:" << arguments.errorCause;
		return 1;
	}
	logArguments(arguments);

	switch (arguments.operation)
	{
	case COMPRESS:
		compressImage(arguments.input, arguments.output, arguments.multiLevel);
		break;
	case DECOMPRESS:
		decompressImage(arguments.input, arguments.output);
		break;
	default:
		break;
	}

	waitKey();
	return 0;
}

void logArguments(Arguments args) {
	std::cout << "Operation: ";
	if (args.operation == COMPRESS) {
		std::cout << "COMPRESS\n";
	}
	if (args.operation == DECOMPRESS) {
		std::cout << "DECOMPRESS\n";
	}
	if (args.multiLevel) {
		std::cout << "Multi Level Threshold true\n";
	}

	std::cout << "Input: " << args.input << "\n";
	std::cout << "Output: " << args.output << "\n";
}

Arguments parseArguments(int argc, char** argv) {
    Arguments data;
	//data.operation = 0;
	//data.input = nullptr;
	//data.output = nullptr;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--compress")) {
            data.operation = COMPRESS;
        }
        else if (!strcmp(argv[i], "--decompress")) {
            data.operation = DECOMPRESS;
        }
        else if (!strcmp(argv[i], "--input")) {
			data.input = argv[++i];
        }
        else if (!strcmp(argv[i], "--output")) {
			data.output = argv[++i];
        }

		else if (!strcmp(argv[i], "--multiLevel")) {
			data.multiLevel = true;
		}
    }

	if (data.operation == 0) {
		data.error = true;
		data.errorCause = "Invalid or missing operation";
	} else if (data.input == nullptr) {
		data.error = true;
		data.errorCause = "Invalid or missing input file path";
	} else if (data.output == nullptr) {
		if (data.operation == COMPRESS) {
			data.output= "compressedImage.rle";
		} else if (data.operation == DECOMPRESS) {
			data.error = true;
			data.errorCause = "Invalid or missing output file path";
		}
	}

    return data;
}

void writeValues(std::ofstream *out, uchar* color, uchar* occurrence) {
	//std::cout << "Color: " << (int)*color << "\nOccurrence: " << (int)*occurrence << "\n\n";

	out->write((char*)color, 1);
	out->write((char*)occurrence, 1);

	*occurrence = 0;
}

void computePDF(Mat img, float pdf[256]) {
	int M = img.rows * img.cols;
	for (int i = 0; i < 256; i++) {
		pdf[i] = 0.0f;
	}

	for (int i = 0; i < img.rows; i++)
	{
		for (int j = 0; j < img.cols; j++)
		{
			pdf[img.at<uchar>(i, j)]++;
		}
	}

	for (int i = 0; i < 256; i++) {
		pdf[i] = (float)(pdf[i] / M);
	}
}

Mat multiLevelThresholding(Mat src) {

	int windowWidth = 5;
	float threshold = 0.0003;

	Mat dst = Mat(src.rows, src.cols, CV_8UC1);
	int max[256];
	max[0] = 0;
	int nrMax = 1;

	float avgV;

	float pdf[256];
	computePDF(src, pdf);
	for (int k = windowWidth; k < 255 - windowWidth; k++)
	{
		avgV = 0.0f;
		for (int i = k - windowWidth; i <= k + windowWidth; i++) {
			avgV += pdf[i];
		}
		avgV /= 2 * windowWidth + 1;

		if (pdf[k] <= (avgV + threshold)) {
			continue;
		}

		float localMax = 0.0f;
		for (int x = k - windowWidth; x <= k + windowWidth; x++) {
			if (localMax < pdf[x]) {
				localMax = pdf[x];
			}
		}

		if (pdf[k] != localMax) {
			continue;
		}

		max[nrMax++] = k;
	}
	max[nrMax++] = 255;

	float min = 256;
	int value, tempVal;
	int minPosition;
	for (int i = 0; i < src.rows; i++) {
		for (int j = 0; j < src.cols; j++) {
			value = src.at<uchar>(i, j);
			min = 256;
			for (int k = 0; k < nrMax; k++) {
				tempVal = abs(value - max[k]);
				if (min > tempVal) {
					min = tempVal;
					minPosition = k;
				}
			}

			dst.at<uchar>(i, j) = max[minPosition];
		}
	}

	return dst;
}

void compressImage(char* input, char* output, boolean multiLevel)
{
	Mat src = imread(input, IMREAD_GRAYSCALE);
	if (src.rows > 65536 || src.cols > 65536) {
		std::cout << "Image is too big (Limit is 65536 x 65536)\n";
		exit(1);
	}
	
	std::ofstream out(output, std::ios::out | std::ios::binary);
	if (!out) {
		exit(1);
	}

	out.write((char*)&src.rows, 2);
	out.write((char*)&src.cols, 2);

	if (multiLevel) {
		src = multiLevelThresholding(src);
	}

	uchar currentColor = -1, currentPixel;
	uchar currentOccurrence = 0;
	boolean started = false;
	
	for (int i = 0; i < src.rows; i++) {
		for (int j = 0; j < src.cols; j++) {
			currentPixel = src.at<uchar>(i, j);
			if (currentPixel == currentColor && started) {
				if (++currentOccurrence == 255) {
					writeValues(&out, &currentColor, &currentOccurrence);
				}
				continue;
			}

			//New color 
			if (started) {
				writeValues(&out, &currentColor, &currentOccurrence);
			}
			currentColor = currentPixel;
			currentOccurrence = 1;
			started = true;
		}
	}
	if (currentOccurrence != 0) {
		writeValues(&out, &currentColor, &currentOccurrence);
	}

	out.close();
	std::cout << "Successfully compressed";
}

void decompressImage(char* input, char* output)
{
	unsigned short int rows, cols;
	std::ifstream in(input, std::ios::out | std::ios::binary);

	in.read((char*)&rows, 2);
	in.read((char*)&cols, 2);

	Mat dst = Mat(rows, cols, CV_8UC1);
	int i = 0, j = 0;
	uchar currentColor;
	uchar tmp;
	while (i < rows && j < cols) {
		in.read((char*)&currentColor, 1);
		in.read((char*)&tmp, 1);
		int occurrence = (int)tmp;

		while (--occurrence >= 0) {
			dst.at<uchar>(i, j) = currentColor;
			if (++j == cols) {
				i++;
				j = 0;
			}
		}
	}

	in.close();
	std::cout << "Successfully decompressed";
	imshow("result", dst);
	imwrite(output, dst);
	waitKey();
}