

/**
  * @file join.c
  * @date 14 Sep 2021 
  * @brief Contains functions used for implentent minr join funtionality
  */


#include <unistd.h>
#include <libgen.h>
#include <sys/stat.h>

#include "minr.h"
#include "file.h"

/**
 * @brief Append the contents of a file to the end of another file.
 * 
 * @param file Origin of the data to be appended.
 * @param destination path to out file
 */
void file_append(char *file, char *destination)
{
	FILE *f;
	uint64_t size = file_size(file);
	if (!size) return;

	/* Read source into memory */
	f = fopen(file, "r");
	if (!f)
	{
		printf("Cannot open source file %s\n", file);
		exit(EXIT_FAILURE);
	}
	char *buffer = malloc(size);
	uint64_t bytes = fread(buffer, 1, size, f);
	fclose(f);
	if (bytes != size)
	{
		free(buffer);
		printf("Failure reading source file %s\n", file);
		exit(EXIT_FAILURE);
	}

	/* Append data to destination */
	f = fopen(destination, "a");
	if (!f)
	{
		free(buffer);
		printf("Cannot write to destination file %s\n", destination);
		exit(EXIT_FAILURE);
	}
	bytes = fwrite(buffer, 1, size, f);
	fclose(f);
	free(buffer);	
}

/**
 * @brief  If the CSV file does not end with LF, eliminate the last line
 * 
 * @param path string path
 */
void truncate_csv(char *path)
{

	FILE *file = fopen(path, "r");
	if (!file)
	{
		printf("Cannot open source file %s\n", path);
		exit(EXIT_FAILURE);
	}

	/* Obtain file size */
	fseeko64(file, 0, SEEK_END);
	uint64_t size = ftello64(file);

	/* Empty file, it is ok */
	if (!size)
	{
		fclose(file);
		return;
	}

	/* Read last byte */	
	uint8_t last_byte[1] = "\0";
	fseeko64(file, size - 1, SEEK_SET);
	fread(last_byte, 1, 1, file);

	/* Ends with chr(10), it is ok */
	if (*last_byte == 10)
	{
		fclose(file);
		return;
	}

	printf("Truncated %s\n", path);

	int i = 0;
	for (i = size - 1; i > 0; i--)
	{

		fseeko64(file, i, SEEK_SET);
		fread(last_byte, 1, 1, file);
		if (*last_byte == 10) break;
	}

	fclose(file);
	if (i > 0) truncate(path, i + 1);
	return;
}

/**
 * @brief Creates a directory with 0755 permissions.
 * 
 * @param destination destination path
 */
void mkdir_if_not_exist(char *destination)
{
	char *dst_dir = strdup(destination);
	char *dir = dirname(dst_dir);

	if (is_dir(dir))
	{
		free(dst_dir);
		return;
	}

	mkdir(dir, 0755);
	if (!is_dir(dir))
	{
		printf("Cannot create directory %s\n", dst_dir);
		free(dst_dir);
		exit(EXIT_FAILURE);
	}

	free(dst_dir);
}

/**
 * @brief Move a file to a new location by copying byte by byte.
 * If the file already exist it is overwritten.
 * 
 * @param src src path
 * @param dst dst path 
 * @param skip_delete if true the src file is not deleted after the copy is done.
 * @return true success. False otherwise.
 */
bool move_file(char *src, char *dst, bool skip_delete) {
		
	mkdir_if_not_exist(dst);
		
	FILE *srcf = fopen(src, "rb");
	if (!srcf) return false;

	FILE *dstf = fopen(dst, "wb");
	if (!dstf) return false;

	/* Copy byte by byte */
	int byte = 0;
	while (!feof(srcf))
	{
		byte = fgetc(srcf);
		if (feof(srcf)) break;
		fputc(byte, dstf);
	}

	fclose(srcf);
	fclose(dstf);
	if (!skip_delete) unlink(src);
	return true;
}

/**
 * @brief join two binary files
 * 
 * @param source path to the source file
 * @param destination path to destination file
 * @param snippets true if it is a snippet file
 * @param skip_delete true to avoid deletion
 */
void bin_join(char *source, char *destination, bool snippets, bool skip_delete)
{
	/* If source does not exist, no need to join */
	if (!is_file(source)) return;

	if (is_file(destination))
	{
		/* Snippet records should divide by 21 */
		if (snippets) if (file_size(destination) % 21)
		{
			printf("File %s does not contain 21-byte records\n", destination);
			exit(EXIT_FAILURE);
		}
	}

	/* If destination does not exist. Source is moved */
	else
	{
		printf("Moving %s into %s\n", source, destination);
		if (!move_file(source, destination, skip_delete))
		{
			printf("Cannot move file\n");
			exit(EXIT_FAILURE);
		}
		return;
	}

	printf("Joining into %s\n", destination);
	file_append(source, destination);
	if (!skip_delete) unlink(source);
}

/**
 * @brief Join two csv files
 * 
 * @param source path to source file
 * @param destination path to destination file 
 * @param skip_delete true for skip deletion
 */
void csv_join(char *source, char *destination, bool skip_delete)
{
	/* If source does not exist, no need to join */
	if (is_file(source)) truncate_csv(source); else return;

	if (is_file(destination))
	{	
		truncate_csv(destination);
	}

	/* If destination does not exist. Source is moved */
	else
	{
		printf("Moving into %s\n", destination);
		if (!move_file(source, destination, skip_delete))
		{
			printf("Cannot move file\n");
			exit(EXIT_FAILURE);
		}
		return;
	}

	printf("Joining into %s\n", destination);
	file_append(source, destination);
	if (!skip_delete) unlink(source);
}

/**
 * @brief Join two mz sources
 * 
 * @param source paht to source
 * @param destination  path to destination
 * @param skip_delete true to skip deletion
 */
void minr_join_mz(char *source, char *destination, bool skip_delete)
{
	char src_path[MAX_PATH_LEN] = "\0";
	char dst_path[MAX_PATH_LEN] = "\0";

	for (int i = 0; i < 65536; i++)
	{
		sprintf(src_path, "%s/sources/%04x.mz", source, i);
		sprintf(dst_path, "%s/sources/%04x.mz", destination, i);
		bin_join(src_path, dst_path, false, skip_delete);
	}
	sprintf(src_path, "%s/sources", source);
	if (!skip_delete) rmdir(src_path);

	for (int i = 0; i < 65536; i++)
	{
		sprintf(src_path, "%s/notices/%04x.mz", source, i);
		sprintf(dst_path, "%s/notices/%04x.mz", destination, i);
		bin_join(src_path, dst_path, false, skip_delete);
	}
	sprintf(src_path, "%s/notices", source);
	if (!skip_delete) rmdir(src_path);
}

/**
 * @brief  Join two snippets file
 * 
 * @param source path to source
 * @param destination path to destination
 * @param skip_delete true to skip deletion
 */
void minr_join_snippets(char *source, char *destination, bool skip_delete)
{
	char src_path[MAX_PATH_LEN] = "\0";
	char dst_path[MAX_PATH_LEN] = "\0";

	for (int i = 0; i < 256; i++)
	{
		sprintf(src_path, "%s/snippets/%02x.bin", source, i);
		sprintf(dst_path, "%s/snippets/%02x.bin", destination, i);
		bin_join(src_path, dst_path, true, skip_delete);
	}
	sprintf(src_path, "%s/snippets", source);
	if (!skip_delete) rmdir(src_path);
}

/**
 * @brief minr join function. Join the files specified in the job
 * 
 * @param job pointer to mnir job
 */
void minr_join(struct minr_job *job)
{
	char *source = job->join_from;
	char *destination = job->join_to;

	if (!is_dir(source) || !is_dir(destination))
	{
		printf("Source and destination must be mined/ directories\n");
		exit(EXIT_FAILURE);
	}

	if (!strcmp(source, destination))
	{
		printf("Source and destination cannot be the same\n");
		exit(EXIT_FAILURE);
	}

	char src_path[MAX_PATH_LEN] = "\0";
	char dst_path[MAX_PATH_LEN] = "\0";

	/* Join urls */
	sprintf(src_path, "%s/urls.csv", source);
	sprintf(dst_path, "%s/urls.csv", destination);
	csv_join(src_path, dst_path, job->skip_delete);

	/* Join files */
	for (int i = 0; i < 256; i++)
	{
		sprintf(src_path, "%s/files/%02x.csv", source, i);
		sprintf(dst_path, "%s/files/%02x.csv", destination, i);
		csv_join(src_path, dst_path, job->skip_delete);
	}
	sprintf(src_path, "%s/files", source);
	if (!job->skip_delete) rmdir(src_path);

	/* Join snippets */
	minr_join_snippets(source, destination, job->skip_delete);

	/* Join MZ (sources/ and notices/) */
	minr_join_mz(source, destination, job->skip_delete);

	/* Join licenses */
	sprintf(src_path, "%s/licenses.csv", source);
	sprintf(dst_path, "%s/licenses.csv", destination);
	csv_join(src_path, dst_path, job->skip_delete);

	/* Join dependencies */
	sprintf(src_path, "%s/dependencies.csv", source);
	sprintf(dst_path, "%s/dependencies.csv", destination);
	csv_join(src_path, dst_path, job->skip_delete);

	/* Join quality */
	sprintf(src_path, "%s/quality.csv", source);
	sprintf(dst_path, "%s/quality.csv", destination);
	csv_join(src_path, dst_path, job->skip_delete);

	/* Join copyright */
	sprintf(src_path, "%s/copyrights.csv", source);
	sprintf(dst_path, "%s/copyrights.csv", destination);
	csv_join(src_path, dst_path, job->skip_delete);

	/* Join quality */
	sprintf(src_path, "%s/quality.csv", source);
	sprintf(dst_path, "%s/quality.csv", destination);
	csv_join(src_path, dst_path, job->skip_delete);

	/* Join vulnerabilities */
	sprintf(src_path, "%s/vulnerabilities.csv", source);
	sprintf(dst_path, "%s/vulnerabilities.csv", destination);
	csv_join(src_path, dst_path, job->skip_delete);

	/* Join attribution */
	sprintf(src_path, "%s/attribution.csv", source);
	sprintf(dst_path, "%s/attribution.csv", destination);
	csv_join(src_path, dst_path, job->skip_delete);

	/* Join cryptography */
	sprintf(src_path, "%s/cryptography.csv", source);
	sprintf(dst_path, "%s/cryptography.csv", destination);
	csv_join(src_path, dst_path, job->skip_delete);

	/* Join Extra tables */
	sprintf(src_path, "%s/extra", source);
	sprintf(dst_path, "%s/extra", destination);

	if (is_dir(src_path) && is_dir(destination))
	{
		/* Join files */
		for (int i = 0; i < 256; i++)
		{
			sprintf(src_path, "%s/extra/files/%02x.csv", source, i);
			sprintf(dst_path, "%s/extra/files/%02x.csv", destination, i);
			csv_join(src_path, dst_path, job->skip_delete);
		}

		/* Join Extra sources */
		for (int i = 0; i < 65536; i++)
		{
			sprintf(src_path, "%s/extra/sources/%04x.mz", source, i);
			sprintf(dst_path, "%s/extra/sources/%04x.mz", destination, i);
			bin_join(src_path, dst_path, false, job->skip_delete);
		}
		sprintf(src_path, "%s/sources", source);
		if (!job->skip_delete) rmdir(src_path);
	}

	if (!job->skip_delete) rmdir(source);
}
