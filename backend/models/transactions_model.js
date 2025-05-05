const db = require('../db');

module.exports = {
    create: (transactionData, connection = null) => {
        return new Promise(async (resolve, reject) => {
            let localConnection = null;
            try {
                if (!connection) {
                    localConnection = await db.getConnection();
                    console.log('Acquired connection for transactions.create');
                    connection = localConnection;
                }

                console.log('Executing query: INSERT INTO transactions SET ?', transactionData);
                const [result] = await connection.query('INSERT INTO transactions SET ?', transactionData);
                console.log('transactions.create result:', result);
                resolve(result);
            } catch (error) {
                console.error('Error in transactions.create:', error.message);
                reject(error);
            } finally {
                if (localConnection) {
                    localConnection.release();
                    console.log('Released connection for transactions.create');
                }
            }
        });
    },

    getByAccountId: (accountId) => {
        return new Promise(async (resolve, reject) => {
            let connection;
            try {
                console.log('Attempting to acquire connection for transactions.getByAccountId');
                connection = await db.getConnection();
                console.log('Acquired connection for transactions.getByAccountId');

                console.log('Executing query: SELECT * FROM transactions WHERE account_id = ? ORDER BY transaction_time DESC', accountId);
                const [results] = await connection.query(
                    'SELECT * FROM transactions WHERE account_id = ? ORDER BY transaction_time DESC LIMIT 10',
                    [accountId]
                );
                console.log('Query result:', results);
                resolve(results);
            } catch (error) {
                console.error('Error in transactions.getByAccountId:', error.message);
                reject(error);
            } finally {
                if (connection) {
                    connection.release();
                    console.log('Released connection for transactions.getByAccountId');
                }
            }
        });
    },
    getOne: (transactionId) => {
        return new Promise((resolve, reject) => {
            db.query(
                'SELECT * FROM transactions WHERE transaction_id = ?',
                [transactionId],
                (error, results) => {
                    if (error) return reject(error);
                    resolve(results[0] || null);
                }
            );
        });
    },

    getAll: () => {
        return new Promise((resolve, reject) => {
            db.query(
                'SELECT * FROM transactions ORDER BY transaction_time DESC',
                (error, results) => {
                    if (error) return reject(error);
                    resolve(results);
                }
            );
        });
    },

    update: (transactionId, updates) => {
        return new Promise((resolve, reject) => {
            db.query(
                'UPDATE transactions SET ? WHERE transaction_id = ?',
                [updates, transactionId],
                (error, result) => {
                    if (error) return reject(error);
                    resolve(result);
                }
            );
        });
    },

    delete: (transactionId) => {
        return new Promise((resolve, reject) => {
            db.query(
                'DELETE FROM transactions WHERE transaction_id = ?',
                [transactionId],
                (error, result) => {
                    if (error) return reject(error);
                    resolve(result);
                }
            );
        });
    }
};