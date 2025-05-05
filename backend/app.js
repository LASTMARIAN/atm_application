const express = require('express');
const logger = require('morgan');
const cookieParser = require('cookie-parser');
const db = require('./db');

cardsRouter = require('./routes/cards');
transactionsRoutes = require('./routes/transactions');
accountsRoutes = require('./routes/accounts');
customersRoutes = require('./routes/customers');

const app = express();

app.use(logger('dev'));
app.use(express.json());
app.use(express.urlencoded({ extended: false }));
app.use(cookieParser());

app.use((req, res, next) => {
    console.log(`[${new Date().toISOString()}] Incoming request: ${req.method} ${req.url}`);
    console.log('Request body:', req.body);
    next();
});

app.get('/test', (req, res) => {
    res.status(200).json({ message: 'Server is running' });
});

app.use('/cards', cardsRouter);
app.use('/transactions', transactionsRoutes);
app.use('/accounts', accountsRoutes);
app.use('/customers', customersRoutes);

const port = 3000;
app.listen(port, async () => {
    console.log(`Server running on http://localhost:${port}`);

    try {
        const results = await db.query('SELECT 1');
        console.log('Database connection test successful:', results);
    } catch (error) {
        console.error('Database connection test failed:', error.message);
    }
});

process.on('uncaughtException', (err) => {
    console.error('Uncaught Exception:', err);
});

process.on('unhandledRejection', (reason, promise) => {
    console.error('Unhandled Rejection at:', promise, 'reason:', reason);
});

module.exports = app;