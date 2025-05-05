const mysql = require('mysql2/promise');

const dbConfig = {
    host: 'localhost',
    user: 'root',
    password: '',
    database: 'pankkiautomaatti',
    waitForConnections: true,
    connectionLimit: 10,
    queueLimit: 0
};

const pool = mysql.createPool(dbConfig);

module.exports = {
    query: async (sql, params) => {
        try {
            const [results] = await pool.query(sql, params);
            return results;
        } catch (error) {
            console.error('Database query error:', error.message);
            throw error;
        }
    },
    getConnection: async () => {
        try {
            const connection = await pool.getConnection();
            return connection;
        } catch (error) {
            console.error('Error getting database connection:', error.message);
            throw error;
        }
    },
    pool
};