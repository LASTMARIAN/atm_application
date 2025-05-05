const db = require('../db');

module.exports = {
    create: (cardData) => {
        return new Promise(async (resolve, reject) => {
            let connection;
            try {
                connection = await db.getConnection();
                console.log('Acquired connection for cards.create');
                const result = await connection.query('INSERT INTO cards SET ?', cardData);
                console.log('cards.create result:', result);
                resolve(result);
            } catch (error) {
                console.error('Error in cards.create:', error.message);
                reject(error);
            } finally {
                if (connection) connection.release();
                console.log('Released connection for cards.create');
            }
        });
    },

    getOne: (cardNumber) => {
        return new Promise((resolve, reject) => {
            let connection;
            let timeout; 
            db.getConnection()
                .then(conn => {
                    connection = conn;
                    console.log('Acquired connection for cards.getOne');
                    timeout = setTimeout(() => { 
                        const error = new Error('Database query timed out');
                        console.error(error.message);
                        reject(error);
                    }, 10000);
    
                    console.log('Executing query: SELECT * FROM cards WHERE card_number = ?', cardNumber);
                    return connection.query('SELECT * FROM cards WHERE card_number = ?', [cardNumber]);
                })
                .then(([results]) => {
                    clearTimeout(timeout);
                    console.log('Query result:', results);
                    resolve(results.length > 0 ? results[0] : null);
                })
                .catch(error => {
                    console.error('Error in cards.getOne:', error.message);
                    reject(error);
                })
                .finally(() => {
                    if (connection) {
                        connection.release();
                        console.log('Released connection for cards.getOne');
                    }
                });
        });
    },

    getAll: () => {
        return new Promise(async (resolve, reject) => {
            let connection;
            try {
                connection = await db.getConnection();
                console.log('Acquired connection for cards.getAll');
                const [results] = await connection.query('SELECT * FROM cards');
                console.log('Query result:', results);
                resolve(results);
            } catch (error) {
                console.error('Error in cards.getAll:', error.message);
                reject(error);
            } finally {
                if (connection) connection.release();
                console.log('Released connection for cards.getAll');
            }
        });
    },

    update: (cardNumber, updates) => {
        return new Promise(async (resolve, reject) => {
            let connection;
            try {
                connection = await db.getConnection();
                console.log('Acquired connection for cards.update');
                const result = await connection.query('UPDATE cards SET ? WHERE card_number = ?', [updates, cardNumber]);
                console.log('Update result:', result);
                resolve(result);
            } catch (error) {
                console.error('Error in cards.update:', error.message);
                reject(error);
            } finally {
                if (connection) connection.release();
                console.log('Released connection for cards.update');
            }
        });
    },

    delete: (cardNumber) => {
        return new Promise(async (resolve, reject) => {
            let connection;
            try {
                connection = await db.getConnection();
                console.log('Acquired connection for cards.delete');
                const result = await connection.query('DELETE FROM cards WHERE card_number = ?', [cardNumber]);
                console.log('Delete result:', result);
                resolve(result);
            } catch (error) {
                console.error('Error in cards.delete:', error.message);
                reject(error);
            } finally {
                if (connection) connection.release();
                console.log('Released connection for cards.delete');
            }
        });
    }
};