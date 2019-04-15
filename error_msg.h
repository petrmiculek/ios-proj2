#ifndef ERROR_MSG_H
#define ERROR_MSG_H
/**
 * @author Petr Mičulek
 * @date 27. 4. 2019
 * @title IOS Projekt 2 - Synchronizace procesů
 * @desc River crossing problem implementation in C,
 * 			using processes, semaphores
 * */

/**
 * @brief print warning messsage to stderr in a custom format
 * @param fmt format
 * @param ... content
 */
void warning_msg(const char *fmt, ...);


/**
 * @brief exit after printing warning messsage to stderr in a custom format
 * @param fmt format
 * @param ... content
 */
void error_exit(const char *fmt, ...);
#endif //ERROR_MSG_H
