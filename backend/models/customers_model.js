const db = require('../db');

module.exports = {
    create: (customerData) => {
        return new Promise(async (resolve, reject) => {
            let connection;
            try {
                connection = await db.getConnection();
                console.log('Acquired connection for customers.create');
                const result = await connection.query('INSERT INTO customers SET ?', customerData);
                console.log('customers.create result:', result);
                resolve(result);
            } catch (error) {
                console.error('Error in customers.create:', error.message);
                reject(error);
            } finally {
                if (connection) {
                    connection.release();
                    console.log('Released connection for customers.create');
                }
            }
        });
    },

    getOne: (customerId) => {
        return new Promise(async (resolve, reject) => {
            let connection;
            try {
                console.log('Attempting to acquire connection for customers.getOne');
                connection = await db.getConnection();
                console.log('Acquired connection for customers.getOne');

                const timeout = setTimeout(() => {
                    const error = new Error('Database query timed out');
                    console.error(error.message);
                    reject(error);
                }, 10000);

                console.log('Executing query: SELECT * FROM customers WHERE customer_id = ?', customerId);
                const [results] = await connection.query('SELECT * FROM customers WHERE customer_id = ?', [customerId]);
                clearTimeout(timeout);
                console.log('Query result:', results);
                resolve(results[0] || null);
            } catch (error) {
                console.error('Error in customers.getOne:', error.message);
                reject(error);
            } finally {
                if (connection) {
                    connection.release();
                    console.log('Released connection for customers.getOne');
                }
            }
        });
    },

    getAll: () => {
        return new Promise(async (resolve, reject) => {
            let connection;
            try {
                connection = await db.getConnection();
                console.log('Acquired connection for customers.getAll');
                const [results] = await connection.query('SELECT * FROM customers');
                console.log('Query result:', results);
                resolve(results);
            } catch (error) {
                console.error('Error in customers.getAll:', error.message);
                reject(error);
            } finally {
                if (connection) {
                    connection.release();
                    console.log('Released connection for customers.getAll');
                }
            }
        });
    },

    update: (customerId, updates) => {
        return new Promise(async (resolve, reject) => {
            let connection;
            try {
                connection = await db.getConnection();
                console.log('Acquired connection for customers.update');
                const result = await connection.query('UPDATE customers SET ? WHERE customer_id = ?', [updates, customerId]);
                console.log('Update result:', result);
                resolve(result);
            } catch (error) {
                console.error('Error in customers.update:', error.message);
                reject(error);
            } finally {
                if (connection) {
                    connection.release();
                    console.log('Released connection for customers.update');
                }
            }
        });
    },

    delete: (customerId) => {
        return new Promise(async (resolve, reject) => {
            let connection;
            try {
                connection = await db.getConnection();
                console.log('Acquired connection for customers.delete');
                const result = await connection.query('DELETE FROM customers WHERE customer_id = ?', [customerId]);
                console.log('Delete result:', result);
                resolve(result);
            } catch (error) {
                console.error('Error in customers.delete:', error.message);
                reject(error);
            } finally {
                if (connection) {
                    connection.release();
                    console.log('Released connection for customers.delete');
                }
            }
        });
    }
};