var express = require('express');
const accountsModel = require('../models/account_model');
var router = express.Router();
const { verifyToken } = require('../verifyToken');

router.get('/get_accounts', verifyToken, async (req, res) => {
    try {
        const accounts = await accountsModel.getAll();
        res.status(200).json(accounts.map(row => ({
            account_id: row.account_id,
            customer_id: row.customer_id,
            balance: parseFloat(row.balance)
        })));
    } catch (error) {
        console.error('Virhe:', error);
        res.status(500).json({ error: 'Sisäinen palvelinvirhe' });
    }
});

router.post('/create_accounts', verifyToken, async (req, res) => {
    const { customer_id, balance } = req.body;

    if (!customer_id || balance === undefined) {
        return res.status(400).json({ error: 'customer_id ja balance ovat pakollisia' });
    }

    const newAccount = { customer_id, balance: parseFloat(balance) };

    try {
        const result = await accountsModel.add(newAccount);
        res.status(201).json({
            message: 'Tili luotu onnistuneesti',
            account_id: result.insertId
        });
    } catch (error) {
        console.error('Virhe:', error);
        res.status(500).json({ error: 'Sisäinen palvelinvirhe' });
    }
});

router.put('/:accountId', verifyToken, async (req, res) => {
    const accountId = req.params.accountId;
    const { balance } = req.body;

    if (balance === undefined) {
        return res.status(400).json({ error: 'balance on pakollinen' });
    }

    const updatedAccount = { balance: parseFloat(balance) };

    try {
        const result = await accountsModel.update(accountId, updatedAccount);
        if (result.affectedRows === 0) {
            return res.status(404).json({ error: 'Tiliä ei löydy' });
        }
        res.status(200).json({ message: 'Tili päivitetty onnistuneesti' });
    } catch (error) {
        console.error('Virhe:', error);
        res.status(500).json({ error: 'Sisäinen palvelinvirhe' });
    }
});

router.delete('/:accountId', verifyToken, async (req, res) => {
    const accountId = req.params.accountId;

    try {
        const result = await accountsModel.delete(accountId);
        if (result.affectedRows === 0) {
            return res.status(404).json({ error: 'Tiliä ei löydy' });
        }
        res.status(200).json({ message: 'Tili poistettu onnistuneesti' });
    } catch (error) {
        console.error('Virhe:', error);
        res.status(500).json({ error: 'Sisäinen palvelinvirhe' });
    }
});

module.exports = router;