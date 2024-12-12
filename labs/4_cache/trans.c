/*
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */
#include "cachelab.h"
#include <stdio.h>

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/*
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded.
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N]) {
  int j, i, ii;
  int a, b, c, d, e, f, g, h;

  if (M == 64) {
    for (ii = 0; ii < N - 3; ii += 4) {
      for (j = 0; j < M - 3; j += 4) {
        for (i = ii; i < ii + 4; i++) {
          a = A[i][j];
          b = A[i][j + 1];
          c = A[i][j + 2];
          d = A[i][j + 3];
          B[j][i] = a;
          B[j + 1][i] = b;
          B[j + 2][i] = c;
          B[j + 3][i] = d;
        }
      }
      for (int k = j; k < M; k++) {
        a = A[ii][k];
        b = A[ii + 1][k];
        c = A[ii + 2][k];
        d = A[ii + 3][k];
        B[j][ii] = a;
        B[j][ii + 1] = b;
        B[j][ii + 2] = c;
        B[j][ii + 3] = d;
      }
    }
    for (i = ii; i < N; i++) {
      for (j = 0; j < M; j++) {
        a = A[i][j];
        B[j][i] = a;
      }
    }
  } else {
    for (j = 0; j < M - 8; j += 8) {
      for (i = 0; i < N; i++) {
        a = A[i][j];
        b = A[i][j + 1];
        c = A[i][j + 2];
        d = A[i][j + 3];
        e = A[i][j + 4];
        f = A[i][j + 5];
        g = A[i][j + 6];
        h = A[i][j + 7];
        B[j][i] = a;
        B[j + 1][i] = b;
        B[j + 2][i] = c;
        B[j + 3][i] = d;
        B[j + 4][i] = e;
        B[j + 5][i] = f;
        B[j + 6][i] = g;
        B[j + 7][i] = h;
      }
    }
    for (i = 0; i < N; i++) {
      for (int k = j; k < M; k++) {
        a = A[i][k];
        B[k][i] = a;
      }
    }
  }
}

/*
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started.
 */

/*
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N]) {
  int i, j, tmp;

  for (i = 0; i < N; i++) {
    for (j = 0; j < M; j++) {
      tmp = A[i][j];
      B[j][i] = tmp;
    }
  }
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions(void) {
  /* Register your solution function */
  registerTransFunction(transpose_submit, transpose_submit_desc);

  /* Register any additional transpose functions */
  registerTransFunction(trans, trans_desc);
}

/*
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N]) {
  int i, j;

  for (i = 0; i < N; i++) {
    for (j = 0; j < M; ++j) {
      if (A[i][j] != B[j][i]) {
        return 0;
      }
    }
  }
  return 1;
}
