const db = require('../db');

module.exports = {
    create: (accountData) => {
        return new Promise(async (resolve, reject) => {
            let connection;
            try {
                connection = await db.getConnection();
                console.log('Acquired connection for accounts.create');
                const result = await connection.query('INSERT INTO accounts SET ?', accountData);
                console.log('accounts.create result:', result);
                resolve(result);
            } catch (error) {
                console.error('Error in accounts.create:', error.message);
                reject(error);
            } finally {
                if (connection) connection.release();
                console.log('Released connection for accounts.create');
            }
        });
    },

    getOne: (accountId) => {
        return new Promise(async (resolve, reject) => {
            let connection;
            try {
                connection = await db.getConnection();
                console.log('Acquired connection for accounts.getOne');
                const timeout = setTimeout(() => {
                    const error = new Error('Database query timed out');
                    console.error(error.message);
                    reject(error);
                }, 10000);

                console.log('Executing query: SELECT * FROM accounts WHERE account_id = ?', accountId);
                const [results] = await connection.query('SELECT * FROM accounts WHERE account_id = ?', [accountId]);
                clearTimeout(timeout);
                console.log('Query result:', results);
                resolve(results);
            } catch (error) {
                console.error('Error in accounts.getOne:', error.message);
                reject(error);
            } finally {
                if (connection) connection.release();
                console.log('Released connection for accounts.getOne');
            }
        });
    },

    getAll: () => {
        return new Promise(async (resolve, reject) => {
            let connection;
            try {
                connection = await db.getConnection();
                console.log('Acquired connection for accounts.getAll');
                const [results] = await connection.query('SELECT * FROM accounts');
                console.log('Query result:', results);
                resolve(results);
            } catch (error) {
                console.error('Error in accounts.getAll:', error.message);
                reject(error);
            } finally {
                if (connection) connection.release();
                console.log('Released connection for accounts.getAll');
            }
        });
    },

    update: (accountId, updates) => {
        return new Promise(async (resolve, reject) => {
            let connection;
            try {
                connection = await db.getConnection();
                console.log('Acquired connection for accounts.update');
                const result = await connection.query('UPDATE accounts SET ? WHERE account_id = ?', [updates, accountId]);
                console.log('Update result:', result);
                resolve(result);
            } catch (error) {
                console.error('Error in accounts.update:', error.message);
                reject(error);
            } finally {
                if (connection) connection.release();
                console.log('Released connection for accounts.update');
            }
        });
    },

    delete: (accountId) => {
        return new Promise(async (resolve, reject) => {
            let connection;
            try {
                connection = await db.getConnection();
                console.log('Acquired connection for accounts.delete');
                const result = await connection.query('DELETE FROM accounts WHERE account_id = ?', [accountId]);
                console.log('Delete result:', result);
                resolve(result);
            } catch (error) {
                console.error('Error in accounts.delete:', error.message);
                reject(error);
            } finally {
                if (connection) connection.release();
                console.log('Released connection for accounts.delete');
            }
        });
    }
};