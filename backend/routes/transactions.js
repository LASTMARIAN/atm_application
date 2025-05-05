var express = require('express');
var bcrypt = require('bcrypt');
const accountsModel = require('../models/account_model');
const transactionsModel = require('../models/transactions_model');
const cardsModel = require('../models/card_model');
var router = express.Router();
const { verifyToken } = require('../verifyToken');
const db = require('../db');

router.post('/withdraw', verifyToken, async (req, res) => {
    const { card_number, pin_code, amount } = req.body;

    console.log('Request body:', req.body);

    if (!card_number || !pin_code || !amount) {
        return res.status(400).json({ error: 'card_number, pin_code, and amount are required' });
    }

    const withdrawalAmount = parseFloat(amount);
    if (isNaN(withdrawalAmount) || withdrawalAmount <= 0) {
        console.log('Invalid withdrawal amount:', amount);
        return res.status(400).json({ error: 'Amount must be a positive number' });
    }

    let connection;

    try {
        connection = await db.getConnection();
        await connection.beginTransaction();


        const card = await cardsModel.getOne(card_number);

        console.log('Card:', card);

        if (!card) {
            await connection.rollback();
            return res.status(404).json({ error: 'Kortti ei ole olemassa' });
        }


        if (card.is_blocked) {
            await connection.rollback();
            return res.status(403).json({ error: 'Kortti on estetty' });
        }

        const storedPinHash = card.pin_hash;
        const pinMatch = await bcrypt.compare(pin_code, storedPinHash);

        console.log('PIN match:', pinMatch);

        if (!pinMatch) {

            const newFailedAttempts = (card.failed_pin_attempts || 0) + 1;
            let isBlocked = false;

            if (newFailedAttempts >= 3) {

                await cardsModel.update(card_number, {
                    failed_pin_attempts: newFailedAttempts,
                    is_blocked: 1
                });
                await connection.commit();
                return res.status(403).json({ error: 'Vaara PIN-koodi. Kortti on estetty 3 vaaran yrityksen jalkeen.' });
            } else {
                await cardsModel.update(card_number, {
                    failed_pin_attempts: newFailedAttempts
                });
                await connection.commit();
                return res.status(403).json({ error: 'Vaara PIN-koodi' });
            }
        }

        if (card.failed_pin_attempts > 0) {
            await cardsModel.update(card_number, {
                failed_pin_attempts: 0
            });
        }


        const account = await accountsModel.getOne(card.account_id);

        console.log('Account:', account);

        if (!account || !account.length) {
            await connection.rollback();
            return res.status(404).json({ error: 'Account not found' });
        }

        const rawBalance = account[0].balance;
        const currentBalance = parseFloat(rawBalance);

        console.log('Raw balance from DB:', rawBalance);
        console.log('Parsed currentBalance:', currentBalance);
        console.log('Withdrawal amount:', withdrawalAmount);

        if (isNaN(currentBalance)) {
            console.log('Balance is not a valid number:', rawBalance);
            await connection.rollback();
            return res.status(500).json({ error: 'Invalid balance in database' });
        }

        if (card.card_type === 'credit') {

            if (card.credit_limit === null || card.credit_limit === undefined) {
                await connection.rollback();
                return res.status(400).json({ error: 'Credit card must have a defined credit limit' });
            }

            const creditLimit = parseFloat(card.credit_limit);
            const usedCredit = currentBalance < 0 ? Math.abs(currentBalance) : 0;
            const availableCredit = creditLimit - usedCredit;

            console.log('Credit limit:', creditLimit, 'Used credit:', usedCredit, 'Available credit:', availableCredit);

            if (withdrawalAmount > availableCredit) {
                await connection.rollback();
                return res.status(400).json({ error: 'Riittamattomat varat: Luottoraja ylitetty' });
            }
        } else {

            if (currentBalance < withdrawalAmount) {
                console.log('Insufficient funds - Current balance:', currentBalance, 'Withdrawal amount:', withdrawalAmount);
                await connection.rollback();
                return res.status(400).json({ error: 'Riittamattomat varat' });
            }
        }

        const newBalance = currentBalance - withdrawalAmount;

        await accountsModel.update(card.account_id, {
            balance: newBalance.toFixed(2)
        });

        await transactionsModel.create({
            transaction_time: new Date(),
            summa: -withdrawalAmount,
            account_id: card.account_id
        }, connection);

        await connection.commit();

        res.status(200).json({
            message: 'Withdrawal successful',
            transaction: {
                amount: withdrawalAmount,
                new_balance: newBalance
            }
        });

        connection.release();
    } catch (error) {
        console.error('Virhe:', error);
        if (connection) {
            try {
                await connection.rollback();
            } catch (rollbackError) {
                console.error('Peruutusvirhe:', rollbackError);
            }
            connection.release();
        }
        res.status(500).json({ error: 'Sisainen palvelinvirhe' });
    }
});


router.post('/balance', verifyToken, async (req, res) => {
    const { card_number, pin_code } = req.body;

    console.log('Request body:', req.body);

    if (!card_number || !pin_code) {
        return res.status(400).json({ error: 'card_number and pin_code are required' });
    }

    try {
        const card = await cardsModel.getOne(card_number);
        console.log('Card fetched:', card);

        if (!card) {
            console.log('No card found for card_number:', card_number);
            return res.status(404).json({ error: 'Card not found' });
        }

        const account = await accountsModel.getOne(card.account_id);
        if (!Array.isArray(account) || account.length === 0) {
            console.log('No account found for account_id:', card.account_id);
            return res.status(404).json({ error: 'Account not found' });
        }

        res.status(200).json({
            message: 'Balance retrieved successfully',
            balance: parseFloat(account[0].balance)
        });
    } catch (error) {
        console.error('Virhe:', error);
        res.status(500).json({ error: 'Sisäinen palvelinvirhe' });
    }
});

router.post('/get_transactions', async (req, res) => {
    console.log('Processing /transactions/get_transactions request...');
    const { account_id } = req.body;

    if (!account_id) {
        console.log('Missing account_id in request body');
        return res.status(400).json({ error: 'account_id is required' });
    }

    try {
        console.log('Fetching last 10 transactions for account_id:', account_id);
        const transactions = await transactionsModel.getByAccountId(account_id);
        console.log('Transactions received from model:', transactions);
        console.log('Type of transactions:', Array.isArray(transactions) ? 'Array' : typeof transactions);
        console.log('Number of transactions:', transactions.length);

        if (!Array.isArray(transactions)) {
            console.error('Expected transactions to be an array, got:', transactions);
            return res.status(500).json({ error: 'Sisäinen palvelinvirhe: odotettiin taulukkoa' });
        }

        if (transactions.length === 0) {
            console.log('No transactions found for account_id:', account_id);
            return res.status(200).json([]);
        }

        console.log('Last 10 transactions fetched successfully:', transactions);
        res.status(200).json(transactions);
    } catch (error) {
        console.error('Error in /transactions/get_transactions:', error.message);
        res.status(500).json({ error: 'Sisäinen palvelinvirhe' });
    }
});

router.post('/top_up', async (req, res) => {
    console.log('Processing /transactions/top_up request...');
    const { account_id, amount } = req.body;
    let connection;

    if (!account_id || !amount || amount <= 0) {
        console.log('Missing or invalid account_id or amount in request body');
        return res.status(400).json({ error: 'account_id and a positive amount are required' });
    }

    try {

        connection = await db.getConnection();
        await connection.beginTransaction();


        console.log('Fetching account for account_id:', account_id);
        const account = await accountsModel.getOne(account_id);
        if (!account || account.length === 0) {
            console.log('Account not found');
            await connection.rollback();
            return res.status(404).json({ error: 'Account not found' });
        }


        const newBalance = parseFloat(account[0].balance) + parseFloat(amount);
        console.log('Updating account balance to:', newBalance);

        await accountsModel.update(account_id, { balance: newBalance.toFixed(2) });


        const transactionData = {
            transaction_time: new Date(),
            summa: parseFloat(amount),
            account_id: account_id
        };
        console.log('Recording transaction for top-up...');
        console.log('Transaction data:', transactionData);
        await transactionsModel.create(transactionData, connection);


        await connection.commit();

        console.log('Top-up successful, new balance:', newBalance);
        res.status(200).json({ success: true, newBalance });
    } catch (error) {
        console.error('Error in /transactions/top_up:', error.message);
        if (connection) {
            try {
                await connection.rollback();
            } catch (rollbackError) {
                console.error('Rollback error:', rollbackError);
            }
            connection.release();
        }
        res.status(500).json({ error: 'Sisäinen palvelinvirhe' });
    }
});

router.put('/:transactionId', verifyToken, async (req, res) => {
    const transactionId = req.params.transactionId;
    const { summa, account_id } = req.body;

    if (!summa || !account_id) {
        return res.status(400).json({ error: 'summa ja account_id ovat pakollisia' });
    }

    const updatedTransaction = { summa: parseFloat(summa), account_id };

    try {
        const result = await transactionsModel.update(transactionId, updatedTransaction);
        if (result.affectedRows === 0) {
            return res.status(404).json({ error: 'Transaktiota ei löydy' });
        }

        res.status(200).json({ message: 'Transaktio päivitetty onnistuneesti' });
    } catch (error) {
        console.error('Virhe:', error);
        res.status(500).json({ error: 'Sisäinen palvelinvirhe' });
    }
});

router.delete('/:transactionId', verifyToken, async (req, res) => {
    const transactionId = req.params.transactionId;

    try {
        const result = await transactionsModel.delete(transactionId);
        if (result.affectedRows === 0) {
            return res.status(404).json({ error: 'Transaktiota ei löydy' });
        }

        res.status(200).json({ message: 'Transaktio poistettu onnistuneesti' });
    } catch (error) {
        console.error('Virhe:', error);
        res.status(500).json({ error: 'Sisäinen palvelinvirhe' });
    }
});

module.exports = router;