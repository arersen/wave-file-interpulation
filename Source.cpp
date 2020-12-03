#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <fstream>

using namespace std;

struct riff_header {
	int32_t chunk_id;
	int32_t chunk_size;
	int32_t format;
};

struct sub_chunk_1 {
	int32_t sub_chunk_1_id;
	int32_t sub_chunk_1_size;
	int16_t audio_format;
	int16_t num_channels;
	int32_t sample_rate;
	int32_t byte_rate;
	int16_t block_align;
	int16_t bits_per_sample;
};

struct sub_chunk_2 {
	int32_t sub_chunk_2_id;
	int32_t sub_chunk_2_size;
};

class music
{
public:

	music() : num_samples_(0), data_(nullptr) {}

	explicit music(const string& filename) : num_samples_(0), data_(nullptr) {
		read(filename);
	}
	void read(const string& filename)
	{
		ifstream in(filename, ios::binary);
		if (in.is_open())
		{
			in.read(reinterpret_cast<char*>(&riff_), sizeof riff_);
			in.read(reinterpret_cast<char*>(&chunk_1_), sizeof chunk_1_);
			in.read(reinterpret_cast<char*>(&chunk_2_), sizeof chunk_2_);

			num_samples_ = 8 * chunk_2_.sub_chunk_2_size /
				chunk_1_.num_channels / chunk_1_.bits_per_sample;

			data_ = new int16_t[num_samples_];

			for (auto i = 0; i < num_samples_; ++i) {
				in.read(reinterpret_cast<char*>(&data_[i]), chunk_1_.bits_per_sample / 8);
			}

			in.close();
		}
		else cerr << "Can not open the file " + filename;
	}

	music resize(const double change)
	{
		music resize_music;

		resize_music.riff_ = riff_;
		resize_music.chunk_1_ = chunk_1_;
		resize_music.chunk_2_ = chunk_2_;

		resize_music.num_samples_ = int(num_samples_ * change);
		resize_music.chunk_2_.sub_chunk_2_size *= change;
		resize_music.riff_.chunk_size = 36 +
			resize_music.chunk_2_.sub_chunk_2_size;

		resize_music.data_ = interpolate(data_, num_samples_, change);

		return resize_music;
	}
	
	void write(const string& filename)
	{
		ofstream out(filename, ios::binary);
		
		if (out.is_open()) 
		{
			out.write(reinterpret_cast<char*>(&riff_), sizeof riff_);
			out.write(reinterpret_cast<char*>(&chunk_1_), sizeof chunk_1_);
			out.write(reinterpret_cast<char*>(&chunk_2_), sizeof chunk_2_);

			for (auto i = 0; i < num_samples_; ++i) {
				out.write(reinterpret_cast<char*>(&data_[i]), chunk_1_.bits_per_sample / 8);
			}

			out.close();
		}
		else cerr << "Can not open the file " + filename;
	}

private:

	struct indices {
		indices() = default;

		explicit indices(const double* values) {
			a = values[0]; b = values[1];
			c = values[2]; d = values[3];
		}

		double a, b, c, d;
	};
	
	double* gauss(double** matrix, double* results, const int size) const {
		auto* values = new double[size];
		for (auto i = 0; i < size; ++i) {
			auto value = matrix[i][i];
			 if (value == 0) {
				auto local = i + 1;
				while (value == 0) {
					value = matrix[local++][i];
				}
				swap(matrix[i], matrix[--local]);
				swap(results[i], results[local]);
			}
			for (auto j = i + 1; j < size; ++j) {
				const auto index = -matrix[j][i] / value;
				for (auto k = i; k < size; ++k) {
					matrix[j][k] += matrix[i][k] * index;
				}
				results[j] += results[i] * index;
			}
		}
		for (auto i = size - 1; i >= 0; --i) {
			const auto index = matrix[i][i];
			auto result = results[i];
			for (auto j = i + 1; j < size; ++j) {
				result -= matrix[i][j] * values[j];
			}
			values[i] = result / index;
		}
		return values;
	}

	int16_t* interpolate(const int16_t* data, const int size, const double change)
	{
		auto* data_copy = new int16_t[size];
		for (auto i = 0; i < size; i++) {
			data_copy[i] = data[i];
		}
			
		auto* splines = new indices[size];
		
		auto** first_matrix = new double* [index_count_];
		for (auto i = 0; i < index_count_; ++i) {
			first_matrix[i] = new double[index_count_];
		}
		
		auto* first_result = new double[index_count_];

		for (auto i = 0; i < index_count_; ++i) {
			for (auto j = 0; j < index_count_; ++j) {
				first_matrix[i][j] = !i ? 1 : 0;
			}
		}
		
		first_matrix[1][3] = 1; first_matrix[2][0] = 3;
		first_matrix[2][1] = 2; first_matrix[2][2] = 1;
		first_matrix[3][0] = 6; first_matrix[3][1] = 2;

		first_result[0] = data_copy[1]; first_result[1] = data_copy[0];
		first_result[2] = (data_copy[2] - data_copy[0]) / 2;
		first_result[3] = (data_copy[3] + data_copy[1] - 2 * data_copy[2]) / 2;

		splines[0] = indices(gauss(first_matrix, first_result, index_count_));

		delete_matrix(first_matrix, index_count_);
		delete[] first_result;

		for (auto i = 1; i < size - 1; ++i) 
		{
			auto** matrix = new double* [index_count_];
			
			for (auto j = 0; j < index_count_; ++j) {
				matrix[j] = new double[index_count_];
			}
			
			auto* result = new double[index_count_];
			
			for (auto j = 0; j < index_count_; ++j) {
				for (auto k = 0; k < index_count_; ++k) {
					matrix[j][k] = !j ? 1 : 0;
				}
			}
			matrix[1][3] = 1; matrix[2][2] = 1;
			matrix[3][1] = 2; matrix[3][0] = 3; matrix[3][2] = 1;

			result[0] = data_copy[i + 1];
			result[1] = data_copy[i];
			result[2] = splines[i - 1].a * 3 + 2 *
				splines[i - 1].b + splines[i - 1].c;
			
			if (i != size - 2) {
				result[3] = (data_copy[i + 2] - data_copy[i]) / 2;
			}
			else result[3] = data_copy[i + 1] - data_copy[i];

			splines[i] = indices(gauss(matrix, result, index_count_));

			delete_matrix(matrix, index_count_);
			delete[] result;
		}

		splines[size - 1] = splines[size - 2];
		
		auto* new_data = new int16_t[unsigned(size * change + 10)];
		
		for (auto j = 0; j < size * change; ++j) 
		{
			const auto k = j * (size - 1) / (size * change - 1);
			
			new_data[j] = splines[int(k)].a * pow(k - int(k), 3) +
				splines[int(k)].b * pow(k - int(k), 2) +
				splines[int(k)].c * pow(k - int(k), 1) +
				splines[int(k)].d;
		}

		delete[] splines;
		delete[] data_copy;

		return new_data;
	}

	template<typename T>
	static void delete_matrix(T** matrix, const int size) {
		for (auto i = 0; i < size; ++i) {
			delete[] matrix[i];
		}
		delete[] matrix;
	}
	
	const int index_count_ = 4;
	riff_header riff_ = {};
	sub_chunk_1 chunk_1_ = {};
	sub_chunk_2 chunk_2_ = {};
	int num_samples_;
	int16_t* data_;
};

int main(const int argc, char* argv[])
{
	if (argc < 4) {
		cerr << "Please input all needed information" << endl;
	}
	else {
		music example(argv[1]);
		auto file = example.resize(stod(argv[3]));
		file.write(argv[2]);
	}
	return 0;
}
